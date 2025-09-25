/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted 
     that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../radio/fec.h" 

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "shared_vars_state.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "packets_utils.h"
#include "video_rx_buffers.h"
#include "timers.h"
#include "ruby_rt_station.h"
#include "test_link_params.h"
#include "adaptive_video.h"

extern t_packet_queue s_QueueRadioPacketsHighPrio;

int ProcessorRxVideo::m_siInstancesCount = 0;

void ProcessorRxVideo::oneTimeInit()
{
   m_siInstancesCount = 0;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      g_pVideoProcessorRxList[i] = NULL;
   log_line("[ProcessorRxVideo] Did one time initialization.");
}

ProcessorRxVideo* ProcessorRxVideo::getVideoProcessorForVehicleId(u32 uVehicleId, u32 uVideoStreamIndex)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      if ( g_pVideoProcessorRxList[i]->m_uVideoStreamIndex == uVideoStreamIndex )
         return g_pVideoProcessorRxList[i];
   }
   return NULL;
}

ProcessorRxVideo::ProcessorRxVideo(u32 uVehicleId, u8 uVideoStreamIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   log_line("[ProcessorRxVideo] Created new instance (number %d of %d) for VID: %u, stream %u", m_iInstanceIndex+1, m_siInstancesCount, uVehicleId, uVideoStreamIndex);
   m_uVehicleId = uVehicleId;
   m_uVideoStreamIndex = uVideoStreamIndex;
   m_uLastTimeRequestedRetransmission = 0;
   m_uLastTimeReceivedRetransmission = 0;
   m_uLastTimeCheckedForMissingPackets = 0;
   m_uRequestRetransmissionUniqueId = 0;
   m_TimeLastHistoryStatsUpdate = 0;
   m_TimeLastRetransmissionsStatsUpdate = 0;
   m_uLatestVideoPacketReceiveTime = 0;

   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;

   m_bPaused = false;
   m_bPauseTempRetrUntillANewVideoPacket = true;
   m_uTimeLastResumedTempRetrPause = 0;
   m_bMustParseStream = false;
   m_bWasParsingStream = false;
   m_ParserH264.init();

   m_pVideoRxBuffer = new VideoRxPacketsBuffer(uVideoStreamIndex, 0);
   Model* pModel = findModelWithId(uVehicleId, 201);
   if ( NULL == pModel )
      log_softerror_and_alarm("[ProcessorRxVideo] Can't find model for VID %u", uVehicleId);
   else
      m_pVideoRxBuffer->init(pModel);

   // Add it to the video decode stats shared mem list

   m_iIndexVideoDecodeStats = -1;
   for(int i=0; i<MAX_VIDEO_PROCESSORS; i++)
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == 0 )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uVehicleId )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
   }
   if ( -1 != m_iIndexVideoDecodeStats )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVehicleId = uVehicleId;
      reset_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uVehicleId);
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVideoStreamIndex = uVideoStreamIndex;
   }
}

ProcessorRxVideo::~ProcessorRxVideo()
{
   
   log_line("[ProcessorRxVideo] Video processor deleted for VID %u, video stream %u", m_uVehicleId, m_uVideoStreamIndex);

   m_siInstancesCount--;

   // Remove this processor from video decode stats list
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      memset(&(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats]), 0, sizeof(shared_mem_video_stream_stats));
      for( int i=m_iIndexVideoDecodeStats; i<MAX_VIDEO_PROCESSORS-1; i++ )
         memcpy(&(g_SM_VideoDecodeStats.video_streams[i]), &(g_SM_VideoDecodeStats.video_streams[i+1]), sizeof(shared_mem_video_stream_stats));
      memset(&(g_SM_VideoDecodeStats.video_streams[MAX_VIDEO_PROCESSORS-1]), 0, sizeof(shared_mem_video_stream_stats));
   }
   m_iIndexVideoDecodeStats = -1;
}

bool ProcessorRxVideo::init()
{
   if ( m_bInitialized )
      return true;
   m_bInitialized = true;

   log_line("[ProcessorRxVideo] Initialize video processor Rx instance number %d, for VID %u, video stream index %u", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
      
   resetReceiveState();
   resetOutputState();
   discardRetransmissionsInfo();
  
   log_line("[ProcessorRxVideo] Initialize video processor complete.");
   log_line("[ProcessorRxVideo] ====================================");
   return true;
}

bool ProcessorRxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[ProcessorRxVideo] Uninitialize video processor Rx instance number %d for VID %u, video stream index %d", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);
   
   m_bInitialized = false;
   return true;
}

void ProcessorRxVideo::resetReceiveState()
{
   log_line("[ProcessorRxVideo] Start: Reset video RX state and buffers");
   
   m_InfoLastReceivedVideoPacket.receive_time = 0;
   m_InfoLastReceivedVideoPacket.video_block_index = 0;
   m_InfoLastReceivedVideoPacket.video_block_packet_index = 0;
   m_InfoLastReceivedVideoPacket.stream_packet_idx = 0;
   m_uLatestVideoPacketReceiveTime = 0;
   
   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   m_uTimeIntervalMsForRequestingRetransmissions = 10;

   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   if ( NULL != m_pVideoRxBuffer )
      m_pVideoRxBuffer->emptyBuffers("Reset receiver state.");

   m_uTimeLastVideoStreamChanged = g_TimeNow;

   log_line("[ProcessorRxVideo] End: Reset video RX state and buffers");
   log_line("--------------------------------------------------------");
   log_line("");
}

void ProcessorRxVideo::resetOutputState()
{
   log_line("[ProcessorRxVideo] Reset output state.");
   m_uLastOutputVideoBlockTime = 0;
   m_uLastOutputVideoBlockIndex = MAX_U32;
   m_uLastOutputVideoBlockPacketIndex = MAX_U32;
   m_uLastOutputVideoBlockDataPackets = 5555;
}

void ProcessorRxVideo::resetStateOnVehicleRestart()
{
   log_line("[ProcessorRxVideo] VID %d, video stream %u: Reset state, full, due to vehicle restart.", m_uVehicleId, m_uVideoStreamIndex);
   resetReceiveState();
   resetOutputState();
   m_uRequestRetransmissionUniqueId = 0;
   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;
}

void ProcessorRxVideo::discardRetransmissionsInfo()
{
   if ( NULL != m_pVideoRxBuffer )
      m_pVideoRxBuffer->emptyBuffers("Discard retransmission info");
   resetOutputState();
   m_uLastTimeCheckedForMissingPackets = g_TimeNow;

   m_uLastTopBlockRequested = 0;
   m_iMaxRecvPacketTopBlockWhenRequested = -1;
}

void ProcessorRxVideo::onControllerSettingsChanged()
{
   log_line("[ProcessorRxVideo] VID %u, video stream %u: Controller params changed. Reinitializing RX video state...", m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo]: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   discardRetransmissionsInfo();
   resetReceiveState();
   resetOutputState();
}

void ProcessorRxVideo::pauseProcessing()
{
   m_bPaused = true;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: paused processing.", m_uVehicleId, m_uVideoStreamIndex);
}

void ProcessorRxVideo::resumeProcessing()
{
   m_bPaused = false;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: resumed processing.", m_uVehicleId, m_uVideoStreamIndex);
}      

void ProcessorRxVideo::setMustParseStream(bool bParse)
{
   m_bMustParseStream = bParse;

   if ( m_bMustParseStream )
   {
      m_ParserH264.init();
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
      reset_video_stream_stats_detected_info(pSMVideoStreamInfo);
      log_line("[ProcessorRxVideo] Was set to parse the received video stream.");
   }
}

bool ProcessorRxVideo::isParsingStream()
{
   return m_bMustParseStream;
}

void ProcessorRxVideo::checkAndDiscardBlocksTooOld()
{
   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getCountBlocksInBuffer()) )
      return;

   // Discard blocks that are too old, past retransmission window
   u32 uCutoffTime = g_TimeNow - m_iMilisecondsMaxRetransmissionWindow;
   int iCountSkipped = m_pVideoRxBuffer->discardOldBlocks(uCutoffTime);

   if ( iCountSkipped > 0 )
   {
      log_line("[ProcessorRxVideo] Discarded %d blocks too old (at least %d ms old), last successfull missing packets check for retransmission: %u ms ago",
          iCountSkipped, m_iMilisecondsMaxRetransmissionWindow, g_TimeNow - m_uLastTimeCheckedForMissingPackets );
      g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex] += iCountSkipped;
      if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
      if ( g_TimeNow > g_TimeStart + 5000 )
         g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
   }
}

u32 ProcessorRxVideo:: getLastRetransmissionId()
{
    return m_uRequestRetransmissionUniqueId;
}

u32 ProcessorRxVideo::getLastTimeRequestedRetransmission()
{
   return m_uLastTimeRequestedRetransmission;
}

u32 ProcessorRxVideo::getLastTimeReceivedRetransmission()
{
   return m_uLastTimeReceivedRetransmission;
}

u32 ProcessorRxVideo::getLastTimeVideoStreamChanged()
{
   return m_uTimeLastVideoStreamChanged;
}

u32 ProcessorRxVideo::getLastestVideoPacketReceiveTime()
{
   return m_uLatestVideoPacketReceiveTime;
}

int ProcessorRxVideo::getVideoWidth()
{
   int iVideoWidth = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoWidth = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoWidth = pModel->video_params.iVideoWidth;
   }
   return iVideoWidth;
}

int ProcessorRxVideo::getVideoHeight()
{
   int iVideoHeight = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoHeight = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoHeight = pModel->video_params.iVideoHeight;
   }
   return iVideoHeight;
}

int ProcessorRxVideo::getVideoFPS()
{
   int iFPS = 0;
   if ( -1 != m_iIndexVideoDecodeStats )
      iFPS = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoFPS;
   if ( 0 == iFPS )
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iFPS = pModel->video_params.iVideoFPS;
   }
   return iFPS;
}

int ProcessorRxVideo::getVideoType()
{
   int iVideoType = 0;
   if ( (-1 != m_iIndexVideoDecodeStats ) &&
        (0 != ((g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) ) )
      iVideoType = (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F;
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
      {
         iVideoType = VIDEO_TYPE_H264;
         if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
            iVideoType = VIDEO_TYPE_H265;
      }
   }
   return iVideoType;
}

u32 ProcessorRxVideo::getLastTempRetrPauseResume()
{
   return m_uTimeLastResumedTempRetrPause;
}

int ProcessorRxVideo::periodicLoop(u32 uTimeNow, bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 155);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
      return -1;
     
   controller_runtime_info_vehicle* pCtrlRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, m_uVehicleId);
   if ( (NULL != pCtrlRTInfo) && (NULL != m_pVideoRxBuffer) )
      pCtrlRTInfo->iCountBlocksInVideoRxBuffers = m_pVideoRxBuffer->getCountBlocksInBuffer();

   checkUpdateRetransmissionsState();
   return checkAndRequestMissingPackets(bForceSyncNow);
}

// Returns 1 if a video block has just finished and the flag "Can TX" is set

void ProcessorRxVideo::handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   if ( m_bPaused )
      return;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pBuffer+sizeof(t_packet_header));    
   controller_runtime_info_vehicle* pCtrlRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, pPH->vehicle_id_src);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 170);

   if ( (NULL == m_pVideoRxBuffer) || (NULL == pRuntimeInfo) || (NULL == pModel) )
      return;

   if ( (!pModel->is_spectator) && (! pRuntimeInfo->bIsPairingDone) )
      return;

   #ifdef PROFILE_RX
   //u32 uTimeStart = get_current_timestamp_ms();
   //int iStackIndexStart = m_iRXBlocksStackTopIndex;
   #endif  

   if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   {
      if ( pPHVS->uCurrentBlockIndex == m_uLastTopBlockRequested )
      {
         log_line("[ProcessorRxVideo] Recv org top block video pckt [%u/%u] [eof: %d] after it was requested for retr.",
            pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
            (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1);
      }
      if ( m_bPauseTempRetrUntillANewVideoPacket )
      {
         m_bPauseTempRetrUntillANewVideoPacket = false;
         m_uTimeLastResumedTempRetrPause = g_TimeNow;
         log_line("[ProcessorRxVideo] Cleared flag to temporarly pause retransmissions");
         adaptive_video_reset_time_for_vehicle(m_uVehicleId);
      }
      if ( m_InfoLastReceivedVideoPacket.video_block_index > 50 )
      if ( pPHVS->uCurrentBlockIndex < m_InfoLastReceivedVideoPacket.video_block_index-50 )
      {
         log_line("[ProcessorRxVideo] Video stream restart detected: received video block %u, last received video block was: %u",
            pPHVS->uCurrentBlockIndex, m_InfoLastReceivedVideoPacket.video_block_index);
         discardRetransmissionsInfo();
         resetReceiveState();
         resetOutputState();
      }
      m_InfoLastReceivedVideoPacket.receive_time = g_TimeNow;
      m_InfoLastReceivedVideoPacket.stream_packet_idx = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      m_InfoLastReceivedVideoPacket.video_block_index = pPHVS->uCurrentBlockIndex;
      m_InfoLastReceivedVideoPacket.video_block_packet_index = pPHVS->uCurrentBlockPacketIndex;
   }

   // Discard retransmitted packets that:
   // * Are from before latest video stream resolution change;
   // * Are from before a vehicle restart detected;
   // * We already received the original packet meanwhile
   // Retransmitted packets are sent from vehicle: on controller request or automatically (ie on a missing ACK)

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      m_uLastTimeReceivedRetransmission = g_TimeNow;
      pCtrlRTInfo->uCountAckRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID )
      if ( pPHVS->uStreamInfo == m_uRequestRetransmissionUniqueId )
      {
         u32 uDeltaTime = g_TimeNow - m_uLastTimeRequestedRetransmission;
         controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, pPH->vehicle_id_src, g_SM_RadioStats.radio_interfaces[interfaceNb].assignedLocalRadioLinkId, uDeltaTime);
      }

      bool bDiscard = false;
      bool bBeforeResChange = false;
      if ( (m_pVideoRxBuffer->getBufferBottomIndex() != -1) && (m_pVideoRxBuffer->getBufferBottomVideoBlockIndex() != 0) && (pPHVS->uCurrentBlockIndex < m_pVideoRxBuffer->getBufferBottomVideoBlockIndex()) )
      {
         g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[g_SMControllerRTInfo.iCurrentIndex]++;
         log_line("[ProcessorRxVideo] Discard retr video pckt [%u/%u] [eof: %d] (part of retr id %u) as it's too old (oldest video block in video rx buffer: %u)",
               pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1,
               pPHVS->uStreamInfo, m_pVideoRxBuffer->getBufferBottomVideoBlockIndex());
         bDiscard = true;
      }
      if ( m_pVideoRxBuffer->hasVideoPacket(pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex) ||
           (pPHVS->uCurrentBlockIndex < m_pVideoRxBuffer->getBufferBottomVideoBlockIndex()) )
      {
         g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[g_SMControllerRTInfo.iCurrentIndex]++;
         log_line("[ProcessorRxVideo] Discard retr video pckt [%u/%u] [eof: %d] (part of retr id %u) as it's already received or outputed.",
               pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1,
               pPHVS->uStreamInfo);
         bDiscard = true;
      }
      else if ( pPHVS->uCurrentBlockIndex == m_uLastTopBlockRequested )
      {
         log_line("[ProcessorRxVideo] Recv retr top video block pckt [%u/%u] [eof: %d] that was requested for retr on retr id %u",
               pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1,
               pPHVS->uStreamInfo);
      }
      else
      {
         log_line("[ProcessorRxVideo] Recv retr video pckt [%u/%u] [eof: %d] that was requested for retr on retr id %u",
               pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1,
               pPHVS->uStreamInfo);
      }

      if ( (0 != m_uLastVideoBlockIndexResolutionChange) && (0 != m_uLastVideoBlockPacketIndexResolutionChange) )
      {
         if ( pPHVS->uCurrentBlockIndex < m_uLastVideoBlockIndexResolutionChange )
         {
            bDiscard = true;
            bBeforeResChange = true;
         }
         if ( pPHVS->uCurrentBlockIndex == m_uLastVideoBlockIndexResolutionChange )
         if ( pPHVS->uCurrentBlockPacketIndex < m_uLastVideoBlockPacketIndexResolutionChange )
         {
            bDiscard = true;
            bBeforeResChange = true;
         }
      }
      if ( bDiscard )
      {
         if ( bBeforeResChange )
         log_line("[ProcessorRxVideo] Discard retr video pckt [%u/%u] [eof: %d] (part of retr id %u) as it's received from before a video resolution change (change happened at video block index [%u/%u])",
               pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) ?
               (pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK): -1,
               pPHVS->uStreamInfo, m_uLastVideoBlockIndexResolutionChange, m_uLastVideoBlockPacketIndexResolutionChange);
         return;
      }
   }

   #if defined(RUBY_BUILD_HW_PLATFORM_PI)
   if (((pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
   {
      static u32 s_uTimeLastSendVideoUnsuportedAlarmToCentral = 0;
      if ( g_TimeNow > s_uTimeLastSendVideoUnsuportedAlarmToCentral + 20000 )
      {
         s_uTimeLastSendVideoUnsuportedAlarmToCentral = g_TimeNow;
         send_alarm_to_central(ALARM_ID_UNSUPPORTED_VIDEO_TYPE, pPHVS->uVideoStreamIndexAndType, pPH->vehicle_id_src);
      }
   }
   #endif

   bool bNewestOnStream = m_pVideoRxBuffer->checkAddVideoPacket(pBuffer, length);
   if ( bNewestOnStream )
   if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   {
      m_uLatestVideoPacketReceiveTime = g_TimeNow;
      updateControllerRTInfoAndVideoDecodingStats(pBuffer, length);
   }

   // If one way link, or retransmissions are off, 
   //    or spectator mode, or not paired yet, or test link is in progress
   //    or negociate radio link is in progress,
   //    or vehicle has lost link to controller,
   //
   // then skip blocks, if there are more video blocks with gaps in buffer

   checkUpdateRetransmissionsState(); // updates bIsDoingRetransmissions state

   outputAvailablePackets(! pRuntimeInfo->bIsDoingRetransmissions);
}

void ProcessorRxVideo::outputAvailablePackets(bool bSkipIncompleteBlocks)
{
   type_rx_video_block_info* pVideoBlock = NULL;
   type_rx_video_packet_info* pVideoPacket = NULL;

   while ( m_pVideoRxBuffer->getCountBlocksInBuffer() != 0 )
   {
      while ( bSkipIncompleteBlocks )
      {
         if ( m_pVideoRxBuffer->discardBottomBlockIfIncomplete() )
         {
            g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
            if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
            if ( g_TimeNow > g_TimeStart + 5000 )
               g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
         }
         else
            break;
      }

      if ( m_pVideoRxBuffer->getCountBlocksInBuffer() == 0 )
         break;

      pVideoPacket = m_pVideoRxBuffer->getBottomBlockAndPacketInBuffer(&pVideoBlock);
      
      // Reached an empty packet?
      if ( (0 == pVideoBlock->uReceivedTime) || pVideoPacket->bEmpty )
            break;

      // Output and advance to next video packet, even if empty

      processAndOutputVideoPacket(pVideoBlock, pVideoPacket);
      m_pVideoRxBuffer->advanceBottomPacketInBuffer();
   }
}

void ProcessorRxVideo::processAndOutputVideoPacket(type_rx_video_block_info* pVideoBlock, type_rx_video_packet_info* pVideoPacket)
{
   t_packet_header_video_segment* pPHVS = pVideoPacket->pPHVS;
   t_packet_header_video_segment_important* pPHVSImp = pVideoPacket->pPHVSImp;
   u8* pVideoRawStreamData = pVideoPacket->pVideoData;
   pVideoRawStreamData += sizeof(t_packet_header_video_segment_important);

   int iVideoWidth = getVideoWidth();
   int iVideoHeight = getVideoHeight();

   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
   bool bMustParseStream = false;
   if ( NULL != pSMVideoStreamInfo )
   if ( (pSMVideoStreamInfo->iDetectedFPS <= 0) || (pSMVideoStreamInfo->iDetectedSlices <= 0) || (pSMVideoStreamInfo->iDetectedKeyframeMs <= 0) )
      bMustParseStream = true;
   if ( NULL != pSMVideoStreamInfo )
   if ( (0 == pSMVideoStreamInfo->uDetectedH264Profile) || (0 == pSMVideoStreamInfo->uDetectedH264Level) )
      bMustParseStream = true;

   if ( ! bMustParseStream )
   {
      if ( m_bMustParseStream )
         log_line("[ProcessorRxVideo] Finished parsing the received video stream.");
      m_bMustParseStream = false;
   }

   if ( m_bMustParseStream || bMustParseStream )
   {
      if ( ! m_bWasParsingStream )
      {
         log_line("[ProcessorRxVideo] Started parsing the received video stream.");
         m_ParserH264.init();
      }
      m_bWasParsingStream = true;
      m_ParserH264.parseData(pVideoRawStreamData, pPHVSImp->uVideoDataLength, g_TimeNow);

      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
      if ( NULL != pSMVideoStreamInfo )
      {
         pSMVideoStreamInfo->uDetectedH264Profile = m_ParserH264.getDetectedProfile();
         pSMVideoStreamInfo->uDetectedH264ProfileConstrains = m_ParserH264.getDetectedProfileConstrains();
         pSMVideoStreamInfo->uDetectedH264Level = m_ParserH264.getDetectedLevel();
      
         if ( pSMVideoStreamInfo->iDetectedFPS <= 0 )
         {
            pSMVideoStreamInfo->iDetectedFPS = m_ParserH264.getDetectedFPS();
            if ( 0 != pSMVideoStreamInfo->iDetectedFPS )
               log_line("[ProcessorRxVideo] Detected video stream FPS: %d FPS", pSMVideoStreamInfo->iDetectedFPS);
         }
         if ( pSMVideoStreamInfo->iDetectedSlices <= 0 )
         {
            pSMVideoStreamInfo->iDetectedSlices = m_ParserH264.getDetectedSlices();
            if ( 0 != pSMVideoStreamInfo->iDetectedSlices )
               log_line("[ProcessorRxVideo] Detected video stream slices: %d slices", pSMVideoStreamInfo->iDetectedSlices);
         }

         if ( pSMVideoStreamInfo->iDetectedKeyframeMs <= 0 )
         {
             pSMVideoStreamInfo->iDetectedKeyframeMs = m_ParserH264.getDetectedKeyframeIntervalMs();
             if ( 0 != pSMVideoStreamInfo->iDetectedKeyframeMs )
                log_line("[ProcessorRxVideo] Detected video stream KF interval: %d ms", pSMVideoStreamInfo->iDetectedKeyframeMs);
         }
      }
   }
   else
   {
      if ( m_bWasParsingStream )
         log_line("[ProcessorRxVideo] Stopped parsing the received video stream.");
      m_bWasParsingStream = false;
   }

   rx_video_output_video_data(m_uVehicleId, (pVideoPacket->pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F , iVideoWidth, iVideoHeight, pVideoRawStreamData, pPHVSImp->uVideoDataLength, pVideoPacket->pPH->total_length);

   // Update controller stats

   g_SMControllerRTInfo.uOutputedVideoPackets[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( pVideoPacket->pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      g_SMControllerRTInfo.uOutputedVideoPacketsRetransmitted[g_SMControllerRTInfo.iCurrentIndex]++;
   
   static u32 s_uLastOutputedVideoBlockId = 0;
   if ( s_uLastOutputedVideoBlockId != pVideoBlock->uVideoBlockIndex )
   {
      s_uLastOutputedVideoBlockId = pVideoBlock->uVideoBlockIndex;
      g_SMControllerRTInfo.uOutputedVideoBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
   }

   static u32 s_uLastOutputedVideoBlockIdReconstructed = 0;
   if ( pVideoPacket->bReconstructed )
   if ( s_uLastOutputedVideoBlockIdReconstructed != pVideoBlock->uVideoBlockIndex )
   {
      s_uLastOutputedVideoBlockIdReconstructed = pVideoBlock->uVideoBlockIndex;
      g_SMControllerRTInfo.uOutputedVideoBlocksECUsed[g_SMControllerRTInfo.iCurrentIndex]++;

      if ( (pPHVS->uCurrentBlockECPackets > 2) &&
           (pVideoBlock->iReconstructedECUsed >= pPHVS->uCurrentBlockECPackets-1) )
         g_SMControllerRTInfo.uOutputedVideoBlocksMaxECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      else if ( (pPHVS->uCurrentBlockECPackets > 0) && (pPHVS->uCurrentBlockECPackets <= 2) &&
           (pVideoBlock->iReconstructedECUsed >= pPHVS->uCurrentBlockECPackets) )
         g_SMControllerRTInfo.uOutputedVideoBlocksMaxECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      
      if ( pVideoBlock->iReconstructedECUsed == 1 )
         g_SMControllerRTInfo.uOutputedVideoBlocksSingleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      else if ( pVideoBlock->iReconstructedECUsed == 2 )
         g_SMControllerRTInfo.uOutputedVideoBlocksTwoECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      else
         g_SMControllerRTInfo.uOutputedVideoBlocksMultipleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      pVideoBlock->iReconstructedECUsed = 0;
   }
}

void ProcessorRxVideo::updateControllerRTInfoAndVideoDecodingStats(u8* pRadioPacket, int iPacketLength)
{
   if ( (m_iIndexVideoDecodeStats < 0) || (m_iIndexVideoDecodeStats >= MAX_VIDEO_PROCESSORS) )
      return;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pRadioPacket+sizeof(t_packet_header));    

   //log_line("DBG flag %d: %u", pPHVS->uStreamInfoFlags, pPHVS->uStreamInfo);

   if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile != pPHVS->uCurrentVideoLinkProfile )
   {
      // Video profile changed on the received stream
      // To fix may2025
      /*
      if ( pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ )
      {
         if ( pPHVS->uCurrentVideoLinkProfile > g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile )
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER;
         else
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER;
      }
      else
         g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE;
      */
   }

   memcpy( &(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS), pPHVS, sizeof(t_packet_header_video_segment));

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SIZE )
   if ( 0 != pPHVS->uCurrentBlockIndex )
   {
      if ( (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth != (int)(pPHVS->uStreamInfo & 0xFFFF)) ||
           (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight != (int)((pPHVS->uStreamInfo >> 16) & 0xFFFF)) )
      {
          m_uLastVideoBlockIndexResolutionChange = pPHVS->uCurrentBlockIndex;
          m_uLastVideoBlockPacketIndexResolutionChange = pPHVS->uCurrentBlockPacketIndex;
      }
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth = pPHVS->uStreamInfo & 0xFFFF;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight = (pPHVS->uStreamInfo >> 16) & 0xFFFF;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_FPS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoFPS = pPHVS->uStreamInfo;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_FEC_TIME )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentFECTimeMicros = pPHVS->uStreamInfo;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentVideoProfileEncodingFlags = pPHVS->uStreamInfo;
   }

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SET_VIDEO_BITRATE )
   {
      if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate != pPHVS->uStreamInfo )
         log_line("[ProcessorRxVideo] Detected video stream set bitrate changed. From %.3f Mbps to %.3f Mbps", (float)g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate/1000.0/1000.0, (float)pPHVS->uStreamInfo/1000.0/1000.0);
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate = pPHVS->uStreamInfo;
   }

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SET_KF_MS )
   {
      if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs != pPHVS->uStreamInfo )
         log_line("[ProcessorRxVideo] Detected video stream set keyframe changed. From %u ms to %u ms", g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs, pPHVS->uStreamInfo);
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs = pPHVS->uStreamInfo;
   }
}

void ProcessorRxVideo::checkUpdateRetransmissionsState()
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   bool bRetransmissionsState = pRuntimeInfo->bIsDoingRetransmissions;
   _checkUpdateRetransmissionsState();

   if ( bRetransmissionsState != pRuntimeInfo->bIsDoingRetransmissions )
   {
      log_line("[ProcessorRxVideo] Retransmissions state changed from %s to %s", bRetransmissionsState?"on":"off", pRuntimeInfo->bIsDoingRetransmissions?"on":"off");
   }
}

void ProcessorRxVideo::_checkUpdateRetransmissionsState()
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   pRuntimeInfo->bIsDoingRetransmissions = false;

   Model* pModel = findModelWithId(m_uVehicleId, 181);
   if ( NULL == pModel )
      return;

   if ( (! pRuntimeInfo->bIsPairingDone) || (g_TimeNow < pRuntimeInfo->uPairingRequestTime + 100) )
      return;
   if ( pModel->isVideoLinkFixedOneWay() || (!(pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)) )
      return;

   if ( g_bSearching || g_bUpdateInProgress || m_bPaused || pModel->is_spectator || test_link_is_in_progress() || isNegociatingRadioLink() )
      return;

   // Do not request from models older than 11.0
   if ( get_sw_version_build(pModel) < 284 )
      return;

   // If we haven't received any video yet, don't try retransmissions
   if ( (0 == m_uLatestVideoPacketReceiveTime) || (-1 == m_iIndexVideoDecodeStats) )
      return;

   if ( g_TimeNow < g_TimeLastVideoParametersOrProfileChanged + 200 )
      return;

   // If link is lost, do not request retransmissions
   if ( pRuntimeInfo->bIsVehicleFastUplinkFromControllerLost )
      return;

   pRuntimeInfo->bIsDoingRetransmissions = true;
}


int ProcessorRxVideo::checkAndRequestMissingPackets(bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 179);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
      return -1;
   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getCountBlocksInBuffer()) )
      return -1;

   if ( rx_video_out_is_stream_output_disabled() )
      return -1;

   if ( m_bPauseTempRetrUntillANewVideoPacket )
      return -1;

   checkUpdateRetransmissionsState();

   int iVideoProfileNow = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile;
   m_iMilisecondsMaxRetransmissionWindow = ((pModel->video_link_profiles[iVideoProfileNow].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK) >> 8) * 5;

   checkAndDiscardBlocksTooOld();
   if ( 0 == m_pVideoRxBuffer->getCountBlocksInBuffer() )
      return -1;
   if ( ! pRuntimeInfo->bIsDoingRetransmissions )
      return -1;

   if ( adaptive_video_is_paused() )
      return -1;

   // If too much time since we last received a new video packet, then discard the entire rx buffer
   if ( m_iMilisecondsMaxRetransmissionWindow >= 10 + 1500/pModel->video_params.iVideoFPS )
   if ( g_TimeNow >= m_uLatestVideoPacketReceiveTime + m_iMilisecondsMaxRetransmissionWindow - 10 )
   {
      log_line("[ProcessorRxVideo] Discard all video buffer due to no new video packet (last newest recv video packet was %u ms ago) past max retransmission window of %d ms", g_TimeNow - m_uLatestVideoPacketReceiveTime, m_iMilisecondsMaxRetransmissionWindow);
      m_pVideoRxBuffer->emptyBuffers("No new video received since start of retransmission window");
      resetOutputState();
      discardRetransmissionsInfo();
      if ( ! m_bPauseTempRetrUntillANewVideoPacket )
      {
         m_bPauseTempRetrUntillANewVideoPacket = true;
         log_line("[ProcessorRxVideo] Set flag to temporarly pause retransmissions as we have no new video data for %u ms", m_iMilisecondsMaxRetransmissionWindow );
      }
      return -1;
   }

   if ( (!bForceSyncNow) && (0 != g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds) )
   {
      u32 uDelta = (u32)g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds;
      if ( uDelta > 50 )
      if ( uDelta > (u32)m_iMilisecondsMaxRetransmissionWindow )
      if ( g_TimeNow > pRuntimeInfo->uLastTimeReceivedAckFromVehicle + uDelta )
         return -1;
   }

   if ( (!bForceSyncNow) && (g_TimeNow < m_uLastTimeRequestedRetransmission + m_uTimeIntervalMsForRequestingRetransmissions) )
      return -1;

   int iCountBlocks = m_pVideoRxBuffer->getCountBlocksInBuffer();
   if ( 0 == iCountBlocks )
      return -1;

   if ( g_TimeNow >= m_uLastTimeRequestedRetransmission + m_iMilisecondsMaxRetransmissionWindow )
      m_uTimeIntervalMsForRequestingRetransmissions = 10;

   // Request all missing packets except current block which is requested only on some cases

   //#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
   // params after header:
   //   u32: retransmission request id
   //   u8: video stream index
   //   u8: number of video packets requested
   //   (u32+u8)*n = each (video block index + video packet index) requested 

   u32 uTimePrevCheck = m_uLastTimeCheckedForMissingPackets;
   m_uLastTimeCheckedForMissingPackets = g_TimeNow;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = m_uVehicleId;
   if ( m_uVehicleId == 0 || m_uVehicleId == MAX_U32 )
   {
      PH.vehicle_id_dest = pModel->uVehicleId;
      log_softerror_and_alarm("[ProcessorRxVideo] Tried to request retransmissions before having received a video packet.");
   }

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8* pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   u32 uTopVideoBlockIdInBuffer = 0;
   int iTopVideoBlockPacketIndexInBuffer = -1;
   //u32 uTopVideoBlockLastRecvTime = 0;
   u32 uLastRequestedVideoBlockIndex = 0;
   int iLastRequestedVideoBlockPacketIndex = 0;

   char szBufferBlocks[128];
   szBufferBlocks[0] = 0;

   /*
   if ( 0 == iCountBlocks )
      log_line("DBG retr window: empty buff, req %u ms ago", g_TimeNow - m_uLastTimeRequestedRetransmission);
   else
   {
      type_rx_video_block_info* pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(0);
      type_rx_video_block_info* pVideoBlockTop = m_pVideoRxBuffer->getBlockInBufferFromBottom(iCountBlocks-1);
      bool bOutputed = true;
      for( int i=0; i<pVideoBlock->iBlockDataPackets; i++ )
      {
         if ( ! pVideoBlock->packets[i].bOutputed )
         {
            bOutputed = false;
            break;
         }
      }
      log_line("DBG retr window: blocks in buffer: %d, bottom: [%u%s%s] (scheme: %d/%d, recv %d/%d data/ec pckts), top:[%u%s], req %u ms ago",
          iCountBlocks, pVideoBlock->uVideoBlockIndex, bOutputed?" *":"",
          pVideoBlock->bEmpty?" empty":"",
          pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets,
          pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets,
          m_pVideoRxBuffer->getBufferTopVideoBlockIndex(),
          pVideoBlockTop->bEmpty?" empty":"",
          g_TimeNow - m_uLastTimeRequestedRetransmission );
   }
   */

   int iCountPacketsRequested = 0;
   type_rx_video_block_info* pVideoBlock = NULL;

   for( int i=0; i<iCountBlocks-1; i++ )
   {
      pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(i);      
      if ( i < 2 )
      {
         if ( 0 != szBufferBlocks[0] )
            strcat(szBufferBlocks, ", ");
         char szTmp[128];
         sprintf(szTmp, "[%u: %d/%d pckts]", pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets);
         strcat(szBufferBlocks, szTmp);
      }

      int iCountToRequestFromBlock = pVideoBlock->iBlockDataPackets - pVideoBlock->iRecvDataPackets - pVideoBlock->iRecvECPackets;
      if ( pVideoBlock->iBlockDataPackets == 0 )
         iCountToRequestFromBlock = 1;
      if ( iCountToRequestFromBlock <= 0 )
         continue;

      for( int k=0; k<pVideoBlock->iBlockDataPackets+1; k++ )
      {
         if ( NULL == pVideoBlock->packets[k].pRawData )
            continue;
         if ( ! pVideoBlock->packets[k].bEmpty )
            continue;

         uLastRequestedVideoBlockIndex = pVideoBlock->uVideoBlockIndex;
         iLastRequestedVideoBlockPacketIndex = k;
         memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
         pDataInfo += sizeof(u32);
         u8 uPacketIndex = k;
         memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
         pDataInfo += sizeof(u8);

         iCountToRequestFromBlock--;
         iCountPacketsRequested++;
         if ( iCountToRequestFromBlock == 0 )
            break;
         if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
           break;
      }
   
      if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
        break;
   }

   // Check and handle top block
   // Ignore current last video block if it's still receiving packets now or has not received any packet at all

   pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(iCountBlocks-1);
   int iCountToRequestFromBlock = pVideoBlock->iBlockDataPackets - pVideoBlock->iRecvDataPackets - pVideoBlock->iRecvECPackets;

   m_uLastTopBlockRequested = 0;
   m_iMaxRecvPacketTopBlockWhenRequested = -1;

   if ( (0 != pVideoBlock->iBlockDataPackets) && (pVideoBlock->iMaxReceivedDataOrECPacketIndex >= 0) && (iCountToRequestFromBlock > 0) )
   {
      uTopVideoBlockIdInBuffer = pVideoBlock->uVideoBlockIndex;
      iTopVideoBlockPacketIndexInBuffer = pVideoBlock->iMaxReceivedDataOrECPacketIndex;
      //uTopVideoBlockLastRecvTime = pVideoBlock->uReceivedTime;

      if ( 0 != szBufferBlocks[0] )
         strcat(szBufferBlocks, ", ");
      char szTmp[128];
      sprintf(szTmp, "[%u: %d/%d pckts]", pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets);
      strcat(szBufferBlocks, szTmp);

      bool bRequestData = false;

      // Reached EC stage but not enough EC packets to reconstruct
      if ( pVideoBlock->iRecvECPackets > 0 )
      if ( (pVideoBlock->iRecvDataPackets + pVideoBlock->iBlockECPackets) < pVideoBlock->iBlockDataPackets )
      {
          bRequestData = true;
      }

      if ( ! bRequestData )
      if ( pVideoBlock->uReceivedTime < g_TimeNow - DEFAULT_VIDEO_END_FRAME_DETECTION_TIMEOUT )
      if ( pVideoBlock->iRecvDataPackets > 0 )
      {
         bRequestData = true;
      }

      if ( bRequestData )
      {
         for( int k=0; k<pVideoBlock->iBlockDataPackets; k++ )
         {
            if ( NULL == pVideoBlock->packets[k].pRawData )
               continue;
            if ( ! pVideoBlock->packets[k].bEmpty )
               continue;

            uLastRequestedVideoBlockIndex = pVideoBlock->uVideoBlockIndex;
            iLastRequestedVideoBlockPacketIndex = k;
            memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
            pDataInfo += sizeof(u32);
            u8 uPacketIndex = k;
            memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
            pDataInfo += sizeof(u8);

            iCountToRequestFromBlock--;
            iCountPacketsRequested++;
            if ( iCountToRequestFromBlock == 0 )
               break;
            if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
              break;
         }
         m_uLastTopBlockRequested = pVideoBlock->uVideoBlockIndex;
         m_iMaxRecvPacketTopBlockWhenRequested = pVideoBlock->iMaxReceivedDataOrECPacketIndex;
         log_line("[ProcessorRxVideo] Top block (%u, scheme %d/%d, %d blocks in Rx buffer, bottom block is: %u) has %d/%d recv data/ec packets, max recv index: %d, eof: %d, last recv packet %u ms ago, last retr check %u ms ago, last check for video rx %u ms ago, will request %d packets.",
             pVideoBlock->uVideoBlockIndex, pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets,
             iCountBlocks, m_pVideoRxBuffer->getBufferBottomVideoBlockIndex(),
             pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, pVideoBlock->iMaxReceivedDataOrECPacketIndex,
             pVideoBlock->iEndOfFrameDetectedAtPacketIndex,
             g_TimeNow - pVideoBlock->uReceivedTime, g_TimeNow - uTimePrevCheck, g_TimeNow - router_get_last_time_checked_for_video_packets(),
             iCountPacketsRequested);
      }
   }

   if ( iCountPacketsRequested == 0 )
      return 0;

   u8 uCount = iCountPacketsRequested;
   m_uRequestRetransmissionUniqueId++;
   memcpy(packet + sizeof(t_packet_header), (u8*)&m_uRequestRetransmissionUniqueId, sizeof(u32));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32), (u8*)&m_uVideoStreamIndex, sizeof(u8));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), (u8*)&uCount, sizeof(u8));
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   PH.total_length += iCountPacketsRequested*(sizeof(u32) + sizeof(u8)); 
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   if ( g_TimeNow < m_uLastTimeRequestedRetransmission + m_iMilisecondsMaxRetransmissionWindow )
   if ( m_uTimeIntervalMsForRequestingRetransmissions < 40 )
       m_uTimeIntervalMsForRequestingRetransmissions += 5;

   m_uLastTimeRequestedRetransmission = g_TimeNow;

   controller_runtime_info_vehicle* pRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, m_uVehicleId);
   if ( NULL != pRTInfo )
   {
      pRTInfo->uCountReqRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] + uCount > 255 )
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] = 255;
      else
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] += uCount;
   }

   pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   u32 uFirstReqBlockIndex =0;
   memcpy(&uFirstReqBlockIndex, pDataInfo, sizeof(u32));
   if ( 1 == iCountPacketsRequested )
      log_line("[ProcessorRxVideo] * Requested retr id %u from vehicle for 1 packet ([%u/%d])",
         m_uRequestRetransmissionUniqueId, uFirstReqBlockIndex, (int)pDataInfo[sizeof(u32)]);
   else
      log_line("[ProcessorRxVideo] * Requested retr id %u from vehicle for %d packets ([%u/%d]...[%u/%d])",
         m_uRequestRetransmissionUniqueId, iCountPacketsRequested,
         uFirstReqBlockIndex, (int)pDataInfo[sizeof(u32)], uLastRequestedVideoBlockIndex, iLastRequestedVideoBlockPacketIndex);
   
   log_line("[ProcessorRxVideo] * Video blocks in buffer: %d (%s), top/max video block in buffer: [%u/pkt %d] / [%u/pkt %d]",
      iCountBlocks, szBufferBlocks, uTopVideoBlockIdInBuffer, iTopVideoBlockPacketIndexInBuffer, m_pVideoRxBuffer->getBufferTopVideoBlockIndex(), m_pVideoRxBuffer->getTopBufferMaxReceivedVideoBlockPacketIndex());
   //log_line("[ProcessorRxVideo] Max Video block packet received: [%u/%d], top video block last recv time: %u ms ago",
   //   m_pVideoRxBuffer->getBufferTopVideoBlockIndex(), m_pVideoRxBuffer->getTopBufferMaxReceivedVideoBlockPacketIndex(), g_TimeNow - uTopVideoBlockLastRecvTime);

   packets_queue_add_packet(&s_QueueRadioPacketsHighPrio, packet);
   return iCountPacketsRequested;
}

void discardRetransmissionsInfoAndBuffersOnLengthyOp()
{
   log_line("[ProcessorRxVideo] Discard all retransmissions info after a lengthy router operation.");
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      g_pVideoProcessorRxList[i]->discardRetransmissionsInfo();
   }
}