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
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/utils.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_cam_maj.h"
#include "../common/string_utils.h"
#include "adaptive_video.h"
#include "adaptive_video_rubalink.h"
#include "shared_vars.h"
#include "timers.h"
#include "video_tx_buffers.h"
#include "video_sources.h"
#include "test_link_params.h"
#include "packets_utils.h"
#include "negociate_radio.h"

u32 s_uAdaptiveVideoLastRequestIdReceived = MAX_U32;
u32 s_uAdaptiveVideoLastRequestIdReceivedTime = 0;

u8  s_uAdaptiveVideoLastFlagsReceived = 0;
int s_iAdaptiveVideoLastSetKeyframeMS = 0;
u32 s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;
u16 s_uAdaptiveVideoLastSetECScheme = 0;
u8  s_uAdaptiveVideoLastSetDRBoost = 0xFF; // 0xFF none set, use video profile DR boost
u32 s_uLastTimeAdaptiveDebugInfo = 0;

bool s_bAdaptiveVideoIsFocusModeBWActive = false;
u32 s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;

void adaptive_video_init()
{
   log_line("[AdaptiveVideo] Init...");
   u32 uCurrentVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps;
   int iDataRateForVideo = g_pCurrentModel->getRadioDataRateForVideoBitrate(uCurrentVideoBitrate, 0);
   int iDRBoost = -1;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      iDRBoost = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;


   log_line("[AdaptiveVideo] Initial bitrate: %.2f Mbps, datarate for it: %s, video profile (%s) DR boost: %d",
       (float)uCurrentVideoBitrate/1000.0/1000.0, str_format_datarate_inline(iDataRateForVideo),
       str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile), iDRBoost);
   adaptive_video_reset_to_defaults();
   log_line("[AdaptiveVideo] Done init.");
}


void adaptive_video_reset_to_defaults()
{
   log_line("[AdaptiveVideo] Reset to defaults.");
   s_iAdaptiveVideoLastSetKeyframeMS = 0;
   s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;
   s_uAdaptiveVideoLastSetDRBoost = 0xFF;
   s_bAdaptiveVideoIsFocusModeBWActive = false;
   s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
   video_sources_set_temporary_image_saturation_off(false);
   //if ( NULL != g_pCurrentModel )
   //   s_iAdaptiveVideoLastSetKeyframeMS = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.iCurrentVideoProfile);
}

void _adaptive_video_turn_focusmode_bw_on()
{
   if ( negociate_radio_link_is_in_progress() )
      return;
   s_bAdaptiveVideoIsFocusModeBWActive = true;
   s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
   video_sources_set_temporary_image_saturation_off(true);
}

void _adaptive_video_turn_focusmode_bw_off_after(u32 uTimeout)
{
   if ( s_bAdaptiveVideoIsFocusModeBWActive )
      s_uAdaptiveVideoTimeTurnFocusModeBWOff = g_TimeNow + uTimeout;
}

void adaptive_video_reset_requested_keyframe()
{
   s_iAdaptiveVideoLastSetKeyframeMS = 0;
}

void adaptive_video_save_state()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "tmp_adaptive.info");
   FILE* fd = fopen(szFile, "wb");
   if ( NULL == fd )
      return;
   fprintf(fd, "%d %u\n", s_iAdaptiveVideoLastSetKeyframeMS, s_uAdaptiveVideoLastSetVideoBitrateBPS);
   fprintf(fd, "%d\n", (int)s_uAdaptiveVideoLastSetDRBoost);
   fclose(fd);
}

void adaptive_video_load_state()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "tmp_adaptive.info");
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("[AdaptiveVideo] No saved temporary state to restore.");
      return;
   }
   log_line("[AdaptiveVideo] Restore temporary state...");
   int iTmp = 0;
   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
      return;
   if ( 1 != fscanf(fd, "%d", &s_iAdaptiveVideoLastSetKeyframeMS) )
      s_iAdaptiveVideoLastSetKeyframeMS = 0;
   if ( 1 != fscanf(fd, "%u", &s_uAdaptiveVideoLastSetVideoBitrateBPS) )
      s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;

   if ( 1 != fscanf(fd, "%u", &iTmp) )
      s_uAdaptiveVideoLastSetDRBoost = 0;
   else
   {
      s_uAdaptiveVideoLastSetDRBoost = iTmp;
      if ( (s_uAdaptiveVideoLastSetDRBoost != 0xFF) && (s_uAdaptiveVideoLastSetDRBoost > 5) )
         s_uAdaptiveVideoLastSetDRBoost = 5;
   }
   fclose(fd);

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szFile);
   hw_execute_bash_command(szComm, NULL);

   if ( 0 != s_iAdaptiveVideoLastSetKeyframeMS )
      video_sources_set_keyframe(s_iAdaptiveVideoLastSetKeyframeMS);
   log_line("[AdaptiveVideo] Done restoring temporary state.");
}

int adaptive_video_get_current_dr_boost()
{
   if ( 0xFF != s_uAdaptiveVideoLastSetDRBoost )
      return s_uAdaptiveVideoLastSetDRBoost;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      return (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
   return 0;
}

int adaptive_video_get_current_keyframe_ms()
{
   return s_iAdaptiveVideoLastSetKeyframeMS;
}

bool adaptive_video_is_on_lower_video_bitrate()
{
   if ( 0 != s_uAdaptiveVideoLastSetVideoBitrateBPS )
   if ( s_uAdaptiveVideoLastSetVideoBitrateBPS != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps )
      return true;
   return false;
}

void adaptive_video_on_uplink_lost()
{
   log_line("[AdaptiveVideo] On link lost.");
}

void adaptive_video_on_uplink_recovered()
{
   log_line("[AdaptiveVideo] On link recovered.");
}

void adaptive_video_on_end_of_frame()
{

}

void adaptive_video_check_update_params()
{

}

void adaptive_video_periodic_loop()
{
   if ( g_TimeNow >= s_uLastTimeAdaptiveDebugInfo + 5000 )
   {
      s_uLastTimeAdaptiveDebugInfo = g_TimeNow;
      log_line("[AdaptiveVideo] Current video capture set bitrate: %.2f Mbps", (float)video_sources_get_last_set_video_bitrate()/1000.0/1000.0);
      log_line("[AdaptiveVideo] Last settings requested from controller: flags: %s", str_format_adaptive_video_flags(s_uAdaptiveVideoLastFlagsReceived));
      log_line("[AdaptiveVideo] Last settings requested from controller: bitrate: %.2f Mbps, keyframe: %d ms", (float)s_uAdaptiveVideoLastSetVideoBitrateBPS/1000.0/1000.0, s_iAdaptiveVideoLastSetKeyframeMS);
      log_line("[AdaptiveVideo] Last settings requested from controller: DR boost: %d, EC: %d/%d", s_uAdaptiveVideoLastSetDRBoost, s_uAdaptiveVideoLastSetECScheme >> 8, s_uAdaptiveVideoLastSetECScheme & 0xFF);
   }

   if ( (0 != s_uAdaptiveVideoTimeTurnFocusModeBWOff) && (g_TimeNow >= s_uAdaptiveVideoTimeTurnFocusModeBWOff) )
   {
      s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
      s_bAdaptiveVideoIsFocusModeBWActive = false;
      video_sources_set_temporary_image_saturation_off(false);
   }
}

void adaptive_video_on_message_from_controller(u32 uRequestId, u8 uFlags, u32 uVideoBitrate, u16 uECScheme, u8 uStreamIndex, int iRadioDatarate, int iKeyframeMs, u8 uDRBoost)
{
   if ( (uRequestId == s_uAdaptiveVideoLastRequestIdReceived) && (uFlags == s_uAdaptiveVideoLastFlagsReceived) )
   {
      log_line("[AdaptiveVideo] Received duplicate req id %u from CID %u, flags: %s. Ignore it.", uRequestId, g_uControllerId, str_format_adaptive_video_flags(uFlags));
      return;
   }
   log_line("[AdaptiveVideo] Received req id %u from CID %u, flags: %s", uRequestId, g_uControllerId, str_format_adaptive_video_flags(uFlags));
   log_line("[AdaptiveVideo] Previous received req id was: %u, %u ms ago, previous flags: %s",
      s_uAdaptiveVideoLastRequestIdReceived, g_TimeNow - s_uAdaptiveVideoLastRequestIdReceivedTime,
      str_format_adaptive_video_flags(s_uAdaptiveVideoLastFlagsReceived));
   log_line("[AdaptiveVideo] Request params: stream id: %d, video bitrate: %.2f Mbps, datarate: %d, keyframe %d ms, EC: %d/%d, DR boost: %d",
      uStreamIndex, (float)uVideoBitrate/1000.0/1000.0, iRadioDatarate, iKeyframeMs, (uECScheme>>8), (uECScheme & 0xFF), uDRBoost);

   s_uAdaptiveVideoLastRequestIdReceived = uRequestId;
   s_uAdaptiveVideoLastRequestIdReceivedTime = g_TimeNow;
   s_uAdaptiveVideoLastFlagsReceived = uFlags;

   u32 uCurrentVideoBitrate = video_sources_get_last_set_video_bitrate();
   if ( 0 == uCurrentVideoBitrate )
      uCurrentVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps;
   log_line("[AdaptiveVideo] Current state: video bitrate: %.2f Mbps (datarate for it: %s), DR boost: %d of %d, EC scheme: %d/%d",
      (float)uCurrentVideoBitrate/1000.0/1000.0,
      str_format_datarate_inline(g_pCurrentModel->getRadioDataRateForVideoBitrate(uCurrentVideoBitrate, 0)),
      s_uAdaptiveVideoLastSetDRBoost, (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT,
       s_uAdaptiveVideoLastSetECScheme >> 8, s_uAdaptiveVideoLastSetECScheme & 0xFF);

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_KEYFRAME )
   {
      s_iAdaptiveVideoLastSetKeyframeMS = iKeyframeMs;
      video_sources_set_keyframe(iKeyframeMs);
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_BITRATE )
   {
      s_uAdaptiveVideoLastSetVideoBitrateBPS = uVideoBitrate;
      packet_utils_set_adaptive_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS);
      if ( negociate_radio_link_is_in_progress() )
      {
         log_line("[AdaptiveVideo] Negociate radio flow is in progress. Do not change video bitrate now.");
         if ( 0 == s_uAdaptiveVideoLastSetVideoBitrateBPS )
            negociate_radio_set_end_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps);
         else
            negociate_radio_set_end_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS);
      }
      else
      {
         if ( 0 == s_uAdaptiveVideoLastSetVideoBitrateBPS )
            video_sources_set_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo");
         else
            video_sources_set_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo");
         log_line("[AdaptiveVideo] Did set new video bitrate of %.2f Mbps; datarate for new video bitrate: %s",
            (float)video_sources_get_last_set_video_bitrate()/1000.0/1000.0,
            str_format_datarate_inline(g_pCurrentModel->getRadioDataRateForVideoBitrate(video_sources_get_last_set_video_bitrate(), 0)));
      }

      if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BW )
      {
         int iCurrentVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
         bool bOnlyMediumAdaptive = false;
         if ( (g_pCurrentModel->video_link_profiles[iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
            bOnlyMediumAdaptive = true;

         int iDataRateForCurrentVideoBitrate = g_pCurrentModel->getRadioDataRateForVideoBitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS, 0);
         if ( (-1 == iDataRateForCurrentVideoBitrate) || (getDataRatesBPS()[0] == iDataRateForCurrentVideoBitrate) )
            _adaptive_video_turn_focusmode_bw_on();
         else if ( bOnlyMediumAdaptive && ((-2 == iDataRateForCurrentVideoBitrate) || (getDataRatesBPS()[1] == iDataRateForCurrentVideoBitrate)) )
            _adaptive_video_turn_focusmode_bw_on();
         else
            _adaptive_video_turn_focusmode_bw_off_after(1000);
      }
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_EC )
   {
      s_uAdaptiveVideoLastSetECScheme = uECScheme;
      if ( (0 == uECScheme) || (0xFFFF == uECScheme) )
      {
         log_line("[AdaptiveVideo] Reset EC scheme to default for model.");
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(0);
      }
      else
      {
         log_line("[AdaptiveVideo] Set custom EC scheme (%d/%d)", uECScheme >> 8, uECScheme & 0xFF);
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(uECScheme);       
      }
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_DR_BOOST )
   {
      s_uAdaptiveVideoLastSetDRBoost = uDRBoost;
      log_line("[AdaptiveVideo] Set DR boost to %d (of %d)", s_uAdaptiveVideoLastSetDRBoost, (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);
      if ( 0 != s_uAdaptiveVideoLastSetDRBoost )
         _adaptive_video_turn_focusmode_bw_off_after(500);
   }
}
