/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
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

#include <math.h>
#include "video_tx_buffers.h"
#include "video_sources.h"
#include "shared_vars.h"
#include "timers.h"
#include "packets_utils.h"
#include "../common/string_utils.h"
#include "../base/hardware_cam_maj.h"
#include "../radio/fec.h"
#include "adaptive_video.h"
#include "processor_tx_video.h"
#include "processor_relay.h"

#define MAX_PACKETS_TO_SEND_IN_ONE_SLICE 40

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
} type_fec_info;
type_fec_info s_FECDecodeInfo;

int VideoTxPacketsBuffer::m_siVideoBuffersInstancesCount = 0;

u32 s_uTimeTotalFecTimeMicroSec = 0;
u32 s_uTimeFecMicroPerSec = 0;
u32 s_uLastTimeFecCalculation = 0;

VideoTxPacketsBuffer::VideoTxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siVideoBuffersInstancesCount;
   m_siVideoBuffersInstancesCount++;

   m_bOverflowFlag = false;
   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVS = NULL;
      m_VideoPackets[i][k].pPHVSImp = NULL;
      m_VideoPackets[i][k].bEmpty = true;
   }
   m_uCurrentH264FrameIndex = 0;
   m_uCurrentH264NALIndex = 0;
   m_uCurrenltyParsedNAL = 0;
   m_uPreviousParsedNAL = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCountReadyToSend = 0;

   m_uCustomECScheme = 0;
   m_uLastAppliedECSchemeDataPackets = 0;
   m_uLastAppliedECSchemeECPackets = 0;
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   m_uRadioStreamPacketIndex = 0;
   m_iVideoStreamInfoIndex = 0;
   m_iUsableRawVideoDataSize = 0;
   memset(&m_PacketHeaderVideo, 0, sizeof(t_packet_header_video_segment));
   memset(&m_PacketHeaderVideoImportant, 0, sizeof(t_packet_header_video_segment_important));

   m_pLastPacketHeaderVideoFilledIn = &m_PacketHeaderVideo;
   m_pLastPacketHeaderVideoImportantFilledIn = &m_PacketHeaderVideoImportant;
   m_ParserInputH264.init();
   m_uTempNALPresenceFlags = 0;
}

VideoTxPacketsBuffer::~VideoTxPacketsBuffer()
{
   uninit();

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      if ( NULL != m_VideoPackets[i][k].pRawData )
         free(m_VideoPackets[i][k].pRawData);

      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVS = NULL;
      m_VideoPackets[i][k].pPHVSImp = NULL;
      m_VideoPackets[i][k].bEmpty = true;
   }

   m_siVideoBuffersInstancesCount--;
}

bool VideoTxPacketsBuffer::init(Model* pModel)
{
   if ( m_bInitialized )
      return true;
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Invalid model on init.");
      return false;
   }
   log_line("[VideoTxBuffer] Initialize video Tx buffer instance number %d.", m_iInstanceIndex+1);

   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   updateVideoHeader(pModel);
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iCountReadyToSend = 0;
   m_bInitialized = true;
   m_bOverflowFlag = false;
   log_line("[VideoTxBuffer] Initialized video Tx buffer instance number %d.", m_iInstanceIndex+1);
   return true;
}

bool VideoTxPacketsBuffer::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoTxBuffer] Uninitialize video Tx buffer instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}

void VideoTxPacketsBuffer::discardBuffer()
{
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iCountReadyToSend = 0;
   
   log_line("[VideoTxBuffer] Discarded entire buffer.");
}


void VideoTxPacketsBuffer::_checkAllocatePacket(int iBufferIndex, int iPacketIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) || (iPacketIndex < 0) || (iPacketIndex >= MAX_TOTAL_PACKETS_IN_BLOCK) )
      return;
   if ( NULL != m_VideoPackets[iBufferIndex][iPacketIndex].pRawData )
      return;

   u8* pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
   if ( NULL == pRawData )
   {
      log_error_and_alarm("Failed to allocate video buffer at index: [%d/%d]", iPacketIndex, iBufferIndex);
      return;
   }
   m_VideoPackets[iBufferIndex][iPacketIndex].pRawData = pRawData;
   m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData = pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment);
   m_VideoPackets[iBufferIndex][iPacketIndex].pPH = (t_packet_header*)pRawData;
   m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS = (t_packet_header_video_segment*)(pRawData + sizeof(t_packet_header));
   m_VideoPackets[iBufferIndex][iPacketIndex].pPHVSImp = (t_packet_header_video_segment_important*)(pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));
   m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty = true;
}

void VideoTxPacketsBuffer::_fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, bool bIsECPacket, int iRawVideoDataSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame, int iCountPacketsToEOF, int iCountDataPacketsAfter)
{
   m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty = false;

   //------------------------------------
   // Update packet header

   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   memcpy(pCurrentPacketHeader, &m_PacketHeader, sizeof(t_packet_header));

   pCurrentPacketHeader->total_length = sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important);
   pCurrentPacketHeader->total_length += iRawVideoDataSize;

   pCurrentPacketHeader->packet_flags_extended = PACKET_FLAGS_EXTENDED_BIT_SEND_ON_HIGH_CAPACITY_LINK_ONLY;

   if ( bEndOfTransmissionFrame && (iCountPacketsToEOF <= PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK) )
   {
      u16 uEOFDelta = iCountPacketsToEOF;
      pCurrentPacketHeader->packet_flags_extended |= (PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK & uEOFDelta);
      pCurrentPacketHeader->packet_flags_extended |= PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME;
      if ( iCountDataPacketsAfter > 0 )
         pCurrentPacketHeader->packet_flags_extended |= PACKET_FLAGS_EXTENDED_BIT_HAS_DATA_AFTER_VIDEO;
   }

   //-------------------------------------
   // Update packet header video segment

   // Update block EC scheme if needed (if this will be a short video block due to end of frame)
   if ( (!bIsECPacket) && bEndOfTransmissionFrame )
   if ( (iPacketIndex + iCountPacketsToEOF) < m_PacketHeaderVideo.uCurrentBlockDataPackets-1 )
   {
      float fECRate = (float)m_PacketHeaderVideo.uCurrentBlockECPackets/(float)m_PacketHeaderVideo.uCurrentBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = (u8) iPacketIndex + iCountPacketsToEOF + 1;
      float fECPackets = (float)m_PacketHeaderVideo.uCurrentBlockDataPackets * fECRate + 0.001;
      u32 uECPackets = (u32)ceil(fECPackets);

      if ( (fECRate > 0.0001) && (uECPackets == 0) )
         uECPackets = 1;
      if ( uECPackets > m_PacketHeaderVideo.uCurrentBlockDataPackets )
         uECPackets = m_PacketHeaderVideo.uCurrentBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = uECPackets;

      int iPacketPrevIndex = iPacketIndex;
      while ( iPacketPrevIndex > 0 )
      {
         iPacketPrevIndex--;
         if ( NULL == m_VideoPackets[iBufferIndex][iPacketPrevIndex].pRawData )
            break;
         m_VideoPackets[iBufferIndex][iPacketPrevIndex].pPHVS->uCurrentBlockDataPackets = iPacketIndex+1;
         m_VideoPackets[iBufferIndex][iPacketPrevIndex].pPHVS->uCurrentBlockECPackets = uECPackets;
      }
   }

   m_PacketHeaderVideo.uCurrentVideoKeyframeIntervalMs = video_sources_get_last_set_keyframe();
   //m_PacketHeaderVideo.uCurrentVideoKeyframeIntervalMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.iCurrentVideoProfile);

   m_pLastPacketHeaderVideoFilledIn = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS;
   memcpy(m_pLastPacketHeaderVideoFilledIn, &m_PacketHeaderVideo, sizeof(t_packet_header_video_segment));
   m_pLastPacketHeaderVideoFilledIn->uH264FrameIndex = m_uCurrentH264FrameIndex;
   m_pLastPacketHeaderVideoFilledIn->uH264NALIndex = m_uCurrentH264NALIndex;
   m_pLastPacketHeaderVideoFilledIn->uCurrentBlockIndex = m_uNextVideoBlockIndexToGenerate;
   m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex = m_uNextVideoBlockPacketIndexToGenerate;

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   m_pLastPacketHeaderVideoFilledIn->uCurrentVideoLinkProfile = iVideoProfile;
   
   if ( adaptive_video_is_on_lower_video_bitrate() )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;
   else
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;

   m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 &= ~(VIDEO_STATUS_FLAGS2_IS_NAL_START | VIDEO_STATUS_FLAGS2_IS_NAL_END | VIDEO_STATUS_FLAGS2_IS_NAL_I | VIDEO_STATUS_FLAGS2_IS_NAL_P | VIDEO_STATUS_FLAGS2_IS_NAL_OTHER);
   m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= uNALPresenceFlags;
   if ( bEndOfTransmissionFrame && (iCountPacketsToEOF == 0) )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_NAL_END;

   if ( bEndOfTransmissionFrame && bIsECPacket )
   if ( 0 == iCountPacketsToEOF )
   if ( m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex == m_pLastPacketHeaderVideoFilledIn->uCurrentBlockDataPackets + m_pLastPacketHeaderVideoFilledIn->uCurrentBlockECPackets - 1 )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME;

   m_iVideoStreamInfoIndex++;
   m_pLastPacketHeaderVideoFilledIn->uStreamInfo = 0;
   u32 uTmp1, uTmp2;

   switch ( m_iVideoStreamInfoIndex )
   {
      case VIDEO_STREAM_INFO_FLAG_NONE:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = 0;
          break;

      case VIDEO_STREAM_INFO_FLAG_SIZE:
          uTmp1 = (u32)g_pCurrentModel->video_params.iVideoWidth;
          uTmp2 = (u32)g_pCurrentModel->video_params.iVideoHeight;
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (uTmp1 & 0xFFFF) | ((uTmp2 & 0xFFFF)<<16);
          break;

      case VIDEO_STREAM_INFO_FLAG_FPS:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (u32)g_pCurrentModel->video_params.iVideoFPS;
          break;

      case VIDEO_STREAM_INFO_FLAG_FEC_TIME:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = s_uTimeFecMicroPerSec;
          break;

      case VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags;
          break;

      case VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID:
          m_iVideoStreamInfoIndex++;
      case VIDEO_STREAM_INFO_FLAG_SET_VIDEO_BITRATE:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = video_sources_get_last_set_video_bitrate();
          break;

      case VIDEO_STREAM_INFO_FLAG_SET_KF_MS:
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = video_sources_get_last_set_keyframe();
          break;

      default:
          m_iVideoStreamInfoIndex = 0;
          m_pLastPacketHeaderVideoFilledIn->uStreamInfo = 0;
          break;
   }
   m_pLastPacketHeaderVideoFilledIn->uStreamInfoFlags = (u8)m_iVideoStreamInfoIndex;   
   if ( bIsECPacket && bEndOfTransmissionFrame )
   {
      // Each time we add an EC packet:
      // Increase previous packets delta EOTF value in the same video frame
      int iTempPacketIndex = iPacketIndex;
      int iTempBufferIndex = iBufferIndex;
      int iCountDisplaced = 0;
      while ( (m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pRawData != NULL) &&
              (! m_VideoPackets[iTempBufferIndex][iTempPacketIndex].bEmpty) &&
              (m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPHVS != NULL) )
      {
         iTempPacketIndex--;
         if ( iTempPacketIndex < 0 )
         {
            iTempPacketIndex = 0;
            iTempBufferIndex--;
            if ( iTempBufferIndex < 0 )
               iTempBufferIndex = MAX_RXTX_BLOCKS_BUFFER-1;
            if ( m_VideoPackets[iTempBufferIndex][0].bEmpty )
               break;
            if ( NULL == m_VideoPackets[iTempBufferIndex][0].pPHVS )
               break;
            iTempPacketIndex = m_VideoPackets[iTempBufferIndex][0].pPHVS->uCurrentBlockDataPackets +  m_VideoPackets[iTempBufferIndex][0].pPHVS->uCurrentBlockECPackets - 1;
         }
         if ( m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPHVS->uH264FrameIndex != m_uCurrentH264FrameIndex )
            break;
         if ( ! (m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME) )
            break;
         if ( NULL == m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH )
            break;
         u16 uEOFDelta = m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK;
         if ( uEOFDelta < PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK )
         {
            uEOFDelta++;
            m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended &= ~PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK;
            m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended |= (PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK & uEOFDelta);
         }
         else
         {
            m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended &= ~PACKET_FLAGS_EXTENDED_BIT_EOTF_MASK;
            m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended &= ~PACKET_FLAGS_EXTENDED_BIT_END_OF_TRANSMISSION_FRAME;
            m_VideoPackets[iTempBufferIndex][iTempPacketIndex].pPH->packet_flags_extended &= ~PACKET_FLAGS_EXTENDED_BIT_HAS_DATA_AFTER_VIDEO;
         }
         iCountDisplaced++;
      }
   }

   if ( bIsECPacket )
      return;
   
   //---------------------------------------------
   // Update packet header video segment important

   m_pLastPacketHeaderVideoImportantFilledIn = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVSImp;
   memcpy(m_pLastPacketHeaderVideoImportantFilledIn, &m_PacketHeaderVideoImportant, sizeof(t_packet_header_video_segment_important));
   m_pLastPacketHeaderVideoImportantFilledIn->uVideoDataLength = (u16)iRawVideoDataSize;
   m_pLastPacketHeaderVideoImportantFilledIn->uVideoImportantFlags = 0;
   if ( bEndOfTransmissionFrame && (0 == iCountPacketsToEOF) )
   {
      m_pLastPacketHeaderVideoImportantFilledIn->uVideoImportantFlags = VIDEO_IMPORTANT_FLAG_EOF;
      if ( iCountDataPacketsAfter > 0 )
         m_pLastPacketHeaderVideoImportantFilledIn->uVideoImportantFlags = VIDEO_IMPORTANT_FLAG_HAS_DATA_AFTER_EOF;
   }
}

void VideoTxPacketsBuffer::setCustomECScheme(u16 uECScheme)
{
   m_uCustomECScheme = uECScheme;
   updateVideoHeader(g_pCurrentModel);
}

int VideoTxPacketsBuffer::getCurrentTotalBlockPackets()
{
   return m_uNextBlockDataPackets + m_uNextBlockECPackets;
}

void VideoTxPacketsBuffer::updateVideoHeader(Model* pModel)
{
   if ( NULL == pModel )
      return;

   log_line("[VideoTxBuffer] On update video header: before: EC scheme: %d/%d, %d bytes model video packet size, developer mode: %s)",
       m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets,
       m_PacketHeaderVideo.uCurrentBlockPacketSize, (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

   log_line("[VideoTxBuffer] On update video header: last applied EC scheme: %d/%d, custom scheme: %d/%d",
      m_uLastAppliedECSchemeDataPackets, m_uLastAppliedECSchemeECPackets,
       (m_uCustomECScheme >> 8) & 0xFF, m_uCustomECScheme & 0xFF);

   log_line("[VideoTxBuffer] On update video header: before: %d usable raw video bytes, maj NAL size: %d",
       m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());

   // Update status flags
   m_PacketHeaderVideo.uVideoStatusFlags2 = 0;
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
      m_PacketHeaderVideo.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;

   // To fix may 2025
   //if ( adaptive_video_is_in_degraded_state() )
   //   m_PacketHeaderVideo.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;
   //else
      m_PacketHeaderVideo.uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;

   radio_packet_init(&m_PacketHeader, PACKET_COMPONENT_VIDEO | PACKET_FLAGS_BIT_HEADERS_ONLY_CRC, PACKET_TYPE_VIDEO_DATA, STREAM_ID_VIDEO_1);
   m_PacketHeader.vehicle_id_src = pModel->uVehicleId;
   m_PacketHeader.vehicle_id_dest = 0;

   m_uNextBlockPacketSize = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].video_data_length;

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   pModel->convertECPercentageToData(&(pModel->video_link_profiles[iVideoProfile]));

   m_uNextBlockDataPackets = pModel->video_link_profiles[iVideoProfile].iBlockDataPackets;
   m_uNextBlockECPackets = pModel->video_link_profiles[iVideoProfile].iBlockECs;
   if ( (0 != m_uCustomECScheme) && (0xFFFF != m_uCustomECScheme) )
   {
      m_uNextBlockECPackets = m_uCustomECScheme & 0xFF;
      if ( m_uNextBlockECPackets > m_uNextBlockDataPackets )
         m_uNextBlockECPackets = m_uNextBlockDataPackets;
      if ( m_uNextBlockECPackets > MAX_FECS_PACKETS_IN_BLOCK )
         m_uNextBlockECPackets = MAX_FECS_PACKETS_IN_BLOCK;
   }

   log_line("[VideoTxBuffer] Next EC scheme to use: %d/%d (%d bytes), now is on video block index: [%u/%u] for scheme [%u/%u]",
       m_uNextBlockDataPackets, m_uNextBlockECPackets, m_uNextBlockPacketSize,
         m_pLastPacketHeaderVideoFilledIn->uCurrentBlockIndex, m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex,
         m_pLastPacketHeaderVideoFilledIn->uCurrentBlockDataPackets, m_pLastPacketHeaderVideoFilledIn->uCurrentBlockECPackets);

   // Apply right away (we just start a new block now)?
   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_uLastAppliedECSchemeDataPackets = m_uNextBlockDataPackets;
      m_uLastAppliedECSchemeECPackets = m_uNextBlockECPackets;
      
      m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hardware_camera_maj_update_nal_size(g_pCurrentModel, false);
      #endif
      log_line("[VideoTxBuffer] Current EC scheme to use rightaway: %d/%d, %d model video packet bytes", m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets, m_PacketHeaderVideo.uCurrentBlockPacketSize);
      log_line("[VideoTxBuffer] Current usable raw bytes: %d, majestic NAL size now: %d", m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());
   }
   else
      log_line("[VideoTxBuffer] Will use EC scheme at next video block.");

   if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H265<<4); // video stream index is 0
      log_line("[VideoTxBuffer] Set video header as H265 stream");
   }
   else
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H264<<4);
      log_line("[VideoTxBuffer] Set video header as H264 stream");
   }
   
   m_PacketHeaderVideo.uCurrentVideoLinkProfile = iVideoProfile;
   m_PacketHeaderVideo.uStreamInfoFlags = 0;
   m_PacketHeaderVideo.uStreamInfo = 0;
}

void VideoTxPacketsBuffer::fillVideoPacketsFromCSI(u8* pVideoData, int iDataSize, bool bEndOfFrame, int iHasPendingDataPacketsToSend)
{
   if ( (NULL == pVideoData) || (iDataSize <= 0) )
      return;

   if ( NULL != g_pProcessorTxVideo )
      process_data_tx_video_on_new_data(pVideoData, iDataSize);

   int iDataSizeLeft = iDataSize;
   u8* pVideoDataLeft = pVideoData;
   while ( iDataSizeLeft > 0 )
   {
      m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);
      int iSizeLeftToFillInCurrentPacket = m_iUsableRawVideoDataSize - m_iTempVideoBufferFilledBytes;
      int iCountToParseAndAdd = iSizeLeftToFillInCurrentPacket;
      if ( iCountToParseAndAdd > iDataSizeLeft )
         iCountToParseAndAdd = iDataSizeLeft;
      m_ParserInputH264.parseData(pVideoDataLeft, iCountToParseAndAdd, g_TimeNow);
      if ( m_ParserInputH264.lastParseDetectedNALStart() != -1 )
      {
         m_uCurrentH264NALIndex++;
         m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_START;
         if ( m_ParserInputH264.lastParseDetectedNALStart() > 4 )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_END;

         if ( m_ParserInputH264.getCurrentNALType() == 1 )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_P;
         else if ( (m_ParserInputH264.getCurrentNALType() == 7) || (m_ParserInputH264.getCurrentNALType() == 8) )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_OTHER;
         else
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_I;
      }
     
      memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoDataLeft, iCountToParseAndAdd);
      m_iTempVideoBufferFilledBytes += iCountToParseAndAdd;

      iDataSizeLeft -= iCountToParseAndAdd;
      pVideoDataLeft += iCountToParseAndAdd;

      // No more room in temp packet buffer or an end of frame? Add it
      bool bAddPacket = false;
      if ( m_iTempVideoBufferFilledBytes >= m_iUsableRawVideoDataSize )
         bAddPacket = true;
      if ( bEndOfFrame && (iDataSizeLeft == 0) )
      if ( m_ParserInputH264.getCurrentNALType() != 8 )
      if ( m_ParserInputH264.getCurrentNALType() != 7 )
         bAddPacket = true;
      if ( bAddPacket )
      {
         if ( (m_ParserInputH264.getCurrentFrameSlices() % m_ParserInputH264.getDetectedSlices()) == 0 )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_END;
        
         int iCountPacketsToEOF = 0;
         if ( iDataSizeLeft > 0 )
            iCountPacketsToEOF += 1 + iDataSizeLeft/m_iUsableRawVideoDataSize;
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempNALPresenceFlags, bEndOfFrame, iCountPacketsToEOF, iHasPendingDataPacketsToSend);
         
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempNALPresenceFlags = 0;

         if ( iDataSizeLeft <= 0 )
         if ( (m_ParserInputH264.getCurrentFrameSlices() % m_ParserInputH264.getDetectedSlices()) == 0 )
         if ( (m_ParserInputH264.getCurrentNALType() != 7) && (m_ParserInputH264.getCurrentNALType() != 8) )
            m_uCurrentH264FrameIndex++;
      }
   }
}

bool VideoTxPacketsBuffer::fillVideoPacketsFromRTSPPacket(u8* pVideoRawData, int iRawDataSize, bool bSingle, bool bEnd, u32 uNALType, int iHasPendingDataPacketsToSend)
{
   if ( (NULL == pVideoRawData) || (iRawDataSize <= 0) )
      return false;

   m_uPreviousParsedNAL = m_uCurrenltyParsedNAL;
   m_uCurrenltyParsedNAL = uNALType;

   if ( NULL != g_pProcessorTxVideo )
      process_data_tx_video_on_new_data(pVideoRawData, iRawDataSize);

   if ( 0 == m_uTempNALPresenceFlags )
      m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_START;
   if ( (uNALType == 7) || (uNALType == 8) )
      m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_OTHER;
   else if ( uNALType == 1 )
      m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_P;
   else
      m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_I;

   bool bEndOfFrameDetected = false;

   if ( bEnd || bSingle )
   if ( (uNALType != 7) && (uNALType != 8) )
   if ( (m_ParserInputH264.getCurrentFrameSlices() % m_ParserInputH264.getDetectedSlices()) == 0 )
   {
      bEndOfFrameDetected = true;
      m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_END;
   }

   m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

   if ( iRawDataSize > m_iUsableRawVideoDataSize - m_iTempVideoBufferFilledBytes )
   {
      log_line("[VideoTxBuffer] Tried to add a video packet (%d bytes, NAL type: %u) larger than max usable video packet size: %d at pos %d",
         iRawDataSize, uNALType, m_iUsableRawVideoDataSize, m_iTempVideoBufferFilledBytes );
   
      int iCountPacketsToEOF = 1 + (iRawDataSize - (m_iUsableRawVideoDataSize - m_iTempVideoBufferFilledBytes))/m_iUsableRawVideoDataSize;

      if ( m_iTempVideoBufferFilledBytes > 0 )
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempNALPresenceFlags, bEndOfFrameDetected, iCountPacketsToEOF, iHasPendingDataPacketsToSend);
      m_iTempVideoBufferFilledBytes = 0;

      if ( iRawDataSize > m_iUsableRawVideoDataSize )
      {
         log_error_and_alarm("[VideoTxBuffer] Tried to add a video packet (%d bytes) that's larger than max usable video packet size: %d",
            iRawDataSize, m_iUsableRawVideoDataSize );
         m_bOverflowFlag = true;
         return false;
      }
   }

   memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoRawData, iRawDataSize);
   m_iTempVideoBufferFilledBytes += iRawDataSize;

   if ( uNALType == 7 )
      return false;

   _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempNALPresenceFlags, bEndOfFrameDetected, 0, iHasPendingDataPacketsToSend);
   
   m_iTempVideoBufferFilledBytes = 0;
   m_uTempNALPresenceFlags = 0;
   if ( bEnd || bSingle || bEndOfFrameDetected )
      m_uCurrentH264NALIndex++;
   if ( bEndOfFrameDetected )
      m_uCurrentH264FrameIndex++;
   return bEndOfFrameDetected;
}

void VideoTxPacketsBuffer::fillVideoPacketsFromNALFrames(u8* pVideoData, int iDataSize, u32 uNALPresenceFlags, int iHasPendingDataPacketsToSend)
{
   if ( (NULL == pVideoData) || (iDataSize <= 0) )
      return;

   if ( NULL != g_pProcessorTxVideo )
      process_data_tx_video_on_new_data(pVideoData, iDataSize);

   int iCountPacketsAdded = 0;
   int iDataSizeLeft = iDataSize;
   u8* pVideoDataLeft = pVideoData;
   while ( iDataSizeLeft > 0 )
   {
      m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);
      int iSizeLeftToFillInCurrentPacket = m_iUsableRawVideoDataSize - m_iTempVideoBufferFilledBytes;
      int iCountToParseAndAdd = iSizeLeftToFillInCurrentPacket;
      if ( iCountToParseAndAdd > iDataSizeLeft )
         iCountToParseAndAdd = iDataSizeLeft;
     
      memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoDataLeft, iCountToParseAndAdd);
      m_iTempVideoBufferFilledBytes += iCountToParseAndAdd;

      iDataSizeLeft -= iCountToParseAndAdd;
      pVideoDataLeft += iCountToParseAndAdd;

      // No more room in the temp packet buffer or the end of data? Add it (video packet)
      if ( (m_iTempVideoBufferFilledBytes >= m_iUsableRawVideoDataSize) || (iDataSizeLeft == 0) )
      {
         m_uTempNALPresenceFlags = uNALPresenceFlags;
         if ( 0 == iCountPacketsAdded )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_START;
         if ( 0 == iDataSizeLeft )
            m_uTempNALPresenceFlags |= VIDEO_STATUS_FLAGS2_IS_NAL_END;
         if ( 0 != iCountPacketsAdded )
            m_uTempNALPresenceFlags &= ~(VIDEO_STATUS_FLAGS2_IS_NAL_OTHER);

         int iCountPacketsToEOF = 0;
         if ( iDataSizeLeft > 0 )
            iCountPacketsToEOF += 1 + iDataSizeLeft/m_iUsableRawVideoDataSize;
         
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempNALPresenceFlags, true, iCountPacketsToEOF, iHasPendingDataPacketsToSend);
         
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempNALPresenceFlags = 0;
         iCountPacketsAdded++;
      }
   }
   m_uCurrentH264NALIndex++;
   m_uCurrentH264FrameIndex++;
}

void VideoTxPacketsBuffer::_addNewVideoPacket(u8* pRawVideoData, int iRawVideoDataSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame, int iCountPacketsToEOF, int iCountDataPacketsAfter)
{
   if ( (! m_bInitialized) || (NULL == pRawVideoData) || (iRawVideoDataSize <= 0) || (iRawVideoDataSize > MAX_PACKET_PAYLOAD) )
      return;

   _checkAllocatePacket(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill);

   // Started a new video block with different scheme (or last block had a modified scheme)? Set the pending EC scheme
   if ( 0 == m_iNextBufferPacketIndexToFill )
   if ( (m_PacketHeaderVideo.uCurrentBlockPacketSize != m_uNextBlockPacketSize) ||
        (m_PacketHeaderVideo.uCurrentBlockDataPackets != m_uNextBlockDataPackets) ||
        (m_PacketHeaderVideo.uCurrentBlockECPackets != m_uNextBlockECPackets) )
   {
      /*
      log_line("[VideoTxBuffer] Pending update video header before: EC scheme: %d/%d, model video packet size: %d",
          m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets,
          m_PacketHeaderVideo.uCurrentBlockPacketSize);

      log_line("[VideoTxBuffer] Pending update video header before: usable raw video bytes: %d, maj NAL size: %d",
          m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());
      */
      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
      m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);
      m_uLastAppliedECSchemeDataPackets = m_uNextBlockDataPackets;
      m_uLastAppliedECSchemeECPackets = m_uNextBlockECPackets;
      
      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hardware_camera_maj_update_nal_size(g_pCurrentModel, false);
      #endif
   }

   // Started a new video block? Clear the block state
   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      for(int i=0; i<(int)(m_PacketHeaderVideo.uCurrentBlockDataPackets + m_PacketHeaderVideo.uCurrentBlockECPackets); i++)
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
      for(int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++)
         m_VideoPackets[m_iNextBufferIndexToFill][i].bEmpty = true;
   }
   _fillVideoPacketHeaders(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill, false, iRawVideoDataSize, uNALPresenceFlags, bEndOfTransmissionFrame, iCountPacketsToEOF, iCountDataPacketsAfter);

   // Copy video data
   t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pPHVS;
   u8* pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pVideoData;
   pVideoDestination += sizeof(t_packet_header_video_segment_important);

   memcpy(pVideoDestination, pRawVideoData, iRawVideoDataSize);
   
   // Set remaining empty space in packet to 0 as EC uses the good video data packets too.
   pVideoDestination += iRawVideoDataSize;
   int iSizeToZero = MAX_PACKET_TOTAL_SIZE - sizeof(t_packet_header) - sizeof(t_packet_header_video_segment) - sizeof(t_packet_header_video_segment_important);
   iSizeToZero -= iRawVideoDataSize;
   if ( iSizeToZero > 0 )
      memset(pVideoDestination, 0, iSizeToZero);

   // Update state
   m_iNextBufferPacketIndexToFill++;
   m_uNextVideoBlockPacketIndexToGenerate++;
   m_iCountReadyToSend++;

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets )
   if ( pCurrentVideoPacketHeader->uCurrentBlockECPackets > 0 )
   {
      // Compute EC packets
      u8* p_fec_data_packets[MAX_DATA_PACKETS_IN_BLOCK];
      u8* p_fec_data_fecs[MAX_FECS_PACKETS_IN_BLOCK];

      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockDataPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
         p_fec_data_packets[i] = m_VideoPackets[m_iNextBufferIndexToFill][i].pVideoData;
      }
      int iECDelta = pCurrentVideoPacketHeader->uCurrentBlockDataPackets;
      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockECPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i+iECDelta);
         p_fec_data_fecs[i] = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pVideoData;
      }

      u32 tTemp = get_current_timestamp_micros();
      fec_encode(pCurrentVideoPacketHeader->uCurrentBlockPacketSize, p_fec_data_packets, pCurrentVideoPacketHeader->uCurrentBlockDataPackets, p_fec_data_fecs, pCurrentVideoPacketHeader->uCurrentBlockECPackets);
      tTemp = get_current_timestamp_micros() - tTemp;
      s_uTimeTotalFecTimeMicroSec += tTemp;
      if ( 0 == s_uLastTimeFecCalculation )
      {
         s_uTimeFecMicroPerSec = 0;
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }
      else if ( g_TimeNow >= s_uLastTimeFecCalculation + 250 )
      {
         s_uTimeFecMicroPerSec = 4 * s_uTimeTotalFecTimeMicroSec;
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }

      int iECDataSize = pCurrentVideoPacketHeader->uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockECPackets; i++ )
      {
         // Update packet headers
         _fillVideoPacketHeaders(m_iNextBufferIndexToFill, i+iECDelta, true, iECDataSize, uNALPresenceFlags, bEndOfTransmissionFrame, iCountPacketsToEOF, iCountDataPacketsAfter);
         m_iNextBufferPacketIndexToFill++;
         m_uNextVideoBlockPacketIndexToGenerate++;
         m_iCountReadyToSend++;
      }
   }

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
   {
      m_uNextVideoBlockPacketIndexToGenerate = 0;
      m_uNextVideoBlockIndexToGenerate++;
      m_iNextBufferPacketIndexToFill = 0;
      m_iNextBufferIndexToFill++;
      if ( m_iNextBufferIndexToFill >= MAX_RXTX_BLOCKS_BUFFER )
         m_iNextBufferIndexToFill = 0;

      // Buffer is full, discard old packets
      if ( m_iNextBufferIndexToFill == m_iCurrentBufferIndexToSend )
      {
         log_softerror_and_alarm("[VideoTxBuffer] Buffer is full. Discard all blocks. (Packets ready to send: %d)", m_iCountReadyToSend);
         discardBuffer();
         log_softerror_and_alarm("[VideoTxBuffer] Discarded blocks to send. (Packets ready to send now: %d)", m_iCountReadyToSend);
      }

      for(int i=0; i<(int)(m_PacketHeaderVideo.uCurrentBlockDataPackets + m_PacketHeaderVideo.uCurrentBlockECPackets); i++)
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
      for(int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++)
         m_VideoPackets[m_iNextBufferIndexToFill][i].bEmpty = true;
   }
}

bool VideoTxPacketsBuffer::_sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId)
{
   if ( m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty )
      return false;

   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS;
      
   if ( pCurrentPacketHeader->total_length < sizeof(t_packet_header) + sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))
      return false;

   // stream_packet_idx: high 4 bits: stream id (0..15), lower 28 bits: stream packet index
   pCurrentPacketHeader->stream_packet_idx = m_uRadioStreamPacketIndex;
   pCurrentPacketHeader->stream_packet_idx &= PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   pCurrentPacketHeader->stream_packet_idx |= (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   m_uRadioStreamPacketIndex++;

   pCurrentPacketHeader->packet_flags = PACKET_COMPONENT_VIDEO;
   pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_HEADERS_ONLY_CRC;

   if ( 0 == uRetransmissionId )
   {
      pCurrentPacketHeader->packet_flags &= ~PACKET_FLAGS_BIT_RETRANSMITED;
      pCurrentPacketHeader->packet_flags &= ~PACKET_FLAGS_BIT_HIGH_PRIORITY;
      if ( m_iCountReadyToSend > 0 )
         m_iCountReadyToSend--;

      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
      {
         if ( iPacketIndex < pCurrentVideoPacketHeader->uCurrentBlockDataPackets )
            g_VehicleRuntimeInfo.uSentVideoDataPackets[g_VehicleRuntimeInfo.iCurrentIndex]++;
         else
            g_VehicleRuntimeInfo.uSentVideoECPackets[g_VehicleRuntimeInfo.iCurrentIndex]++;
      }
   }
   else
   {
      pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_RETRANSMITED;
      pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
      pCurrentVideoPacketHeader->uStreamInfoFlags = VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID;
      pCurrentVideoPacketHeader->uStreamInfo = uRetransmissionId;
   }

   if ( g_bVideoPaused || (! relay_current_vehicle_must_send_own_video_feeds()) )
      return false;

   //t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*) m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData;
   //u8* pVideoData = m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData;
   //pVideoData += sizeof(t_packet_header_video_full_98_debug_info);
   //u32 crc = base_compute_crc32(pVideoData, pCurrentVideoPacketHeader->uCurrentBlockPacketSize);

   send_packet_to_radio_interfaces((u8*)pCurrentPacketHeader, pCurrentPacketHeader->total_length, -1);
   return true;
}

int VideoTxPacketsBuffer::hasPendingPacketsToSend()
{
   return m_iCountReadyToSend;
}

u32 VideoTxPacketsBuffer::getFirstPacketToSendVideoBlockIndex()
{
   if ( m_iCountReadyToSend <= 0 )
      return 0;
   if ( NULL == m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH )
      return 0xFFFFFFFF;
   if ( m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].bEmpty )
      return 0xFFFFFFFF;
   t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVS;
   return pCurrentVideoPacketHeader->uCurrentBlockIndex;
}


int VideoTxPacketsBuffer::sendAvailablePackets(int iMaxCountToSend)
{
   if ( m_iCountReadyToSend <= 0 )
      return 0;

   int iToSend = m_iCountReadyToSend;
   if ( iToSend > MAX_PACKETS_TO_SEND_IN_ONE_SLICE )
      iToSend = MAX_PACKETS_TO_SEND_IN_ONE_SLICE;
   if ( iMaxCountToSend > 0 )
   if ( iToSend > iMaxCountToSend )
      iToSend = iMaxCountToSend;

   int iCountSent = 0;
   for( int i=0; i<iToSend; i++ )
   {
      if ( NULL == m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH )
      {
         log_softerror_and_alarm("Invalid packet [%d/%d], video next to gen: [%u/%u], ready to send: %d, header: %X", m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend,
            m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate, m_iCountReadyToSend, m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH);
         break;
      }
      if ( m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].bEmpty )
      {
         log_softerror_and_alarm("Try to send empty packet [%d/%d], video next to gen: [%u/%u], ready to send: %d, header: %X", m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend,
            m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate, m_iCountReadyToSend, m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH);
         break;
      }

      _sendPacket(m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend, 0);
      iCountSent++;

      t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVS;
      if ( (pCurrentVideoPacketHeader->uCurrentBlockECPackets == 0) && (pCurrentVideoPacketHeader->uCurrentBlockDataPackets == 1) )
         _sendPacket(m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend, 0);

      m_iCurrentBufferPacketIndexToSend++;

      if ( m_iCurrentBufferPacketIndexToSend >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
      {
         m_iCurrentBufferPacketIndexToSend = 0;
         m_iCurrentBufferIndexToSend++;
         if ( m_iCurrentBufferIndexToSend >= MAX_RXTX_BLOCKS_BUFFER )
            m_iCurrentBufferIndexToSend = 0;
      }
      if ( m_iCountReadyToSend <= 0 )
         break;

      if ( m_iCurrentBufferIndexToSend == m_iNextBufferIndexToFill )
      if ( m_iCurrentBufferPacketIndexToSend == m_iNextBufferPacketIndexToFill )
         break;
   }
   return iCountSent;
}


void VideoTxPacketsBuffer::resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex)
{
   if ( uVideoBlockIndex > m_uNextVideoBlockIndexToGenerate )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv req for retr for block index %u which is greater than max present in buffer: %u",
         uVideoBlockIndex, m_uNextVideoBlockIndexToGenerate-1);
      return;
   }
   if ( uVideoBlockIndex == m_uNextVideoBlockIndexToGenerate )
   if ( uVideoBlockPacketIndex >= m_uNextVideoBlockPacketIndexToGenerate )
   {
      log_line("[VideoTxBuffer] Recv req for retr for block packet [%u/%u] which is greater than max present in buffer: [%u/%u], top video block packet index: %d, is at end of frame: %s",
         uVideoBlockIndex, uVideoBlockPacketIndex, m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate-1,
         (int)m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex,
         "xx");// To fix may 2025 check packet header for end of transmission (m_pLastPacketHeaderVideoImportantFilledIn->uFrameAndNALFlags & VIDEO_PACKET_FLAGS_IS_END_OF_TRANSMISSION_FRAME)?"yes":"no");
      return;
   }
   int iDeltaBlocksBack = (int)m_uNextVideoBlockIndexToGenerate - (int)uVideoBlockIndex;
   if ( (iDeltaBlocksBack < 0) || (iDeltaBlocksBack >= MAX_RXTX_BLOCKS_BUFFER) )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv req for retr for block index out of range: %d blocks back (of max %d blocks)", iDeltaBlocksBack, MAX_RXTX_BLOCKS_BUFFER);
      return;
   }
   int iBufferIndex = m_iNextBufferIndexToFill - iDeltaBlocksBack;
   if ( iBufferIndex < 0 )
      iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;

   // Still too old?
   if ( iBufferIndex < 0 )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr for block index still out of range: %d ", iBufferIndex);
      return;
   }
   if ( NULL == m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPH )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of empty video block index [%u/%u]", uVideoBlockIndex, uVideoBlockPacketIndex);
      return;
   }
   if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex != uVideoBlockIndex )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of invalid video block [%u], buffer has video block [%u] at that position (%d)", uVideoBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, iBufferIndex);
      return;
   }
   if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex != uVideoBlockPacketIndex )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of invalid video block [%u/%u], buffer has video block [%u/%u] at that position (%d)", uVideoBlockIndex, uVideoBlockPacketIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex, iBufferIndex);
      return;
   }

   if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].bEmpty )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of empty video packet [%u/%u], buffer has video block [%u/%u] at that position (%d), next video packet to generate now is: [%u/%u]",
         uVideoBlockIndex, uVideoBlockPacketIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex, iBufferIndex,
         m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate);
      return;
   }
   if ( 0 == uRetransmissionId )
      uRetransmissionId = MAX_U32-1;
   _sendPacket(iBufferIndex, (int)uVideoBlockPacketIndex, uRetransmissionId);
}


u32 VideoTxPacketsBuffer::getCurrentOutputFrameIndex()
{
   return m_uCurrentH264FrameIndex;
}

u32 VideoTxPacketsBuffer::getCurrentOutputNALIndex()
{
   return m_uCurrentH264NALIndex;
}

int VideoTxPacketsBuffer::getCurrentUsableRawVideoDataSize()
{
   return m_iUsableRawVideoDataSize;
}

bool VideoTxPacketsBuffer::getResetOverflowFlag()
{
   bool bRet = m_bOverflowFlag;
   m_bOverflowFlag = false;
   return bRet;
}

int VideoTxPacketsBuffer::getCurrentMaxUsableRawVideoDataSize()
{
   return m_iUsableRawVideoDataSize;
}
