#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_radio.h"
#include "shared_vars.h"
#include <math.h>

float g_fOSDDbm[MAX_RADIO_INTERFACES];
float g_fOSDSNR[MAX_RADIO_INTERFACES];
u32   g_uOSDDbmLastCaptureTime[MAX_RADIO_INTERFACES];


void shared_vars_osd_reset_before_pairing()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_fOSDDbm[i] = -200.0f;
      g_fOSDSNR[i] = 0.0;
      g_uOSDDbmLastCaptureTime[i] = 0;
   }
}

void shared_vars_osd_update()
{
   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! pRadioHWInfo->isHighCapacityInterface )
         continue;
      
      if ( (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMVideoForInterface > -500) && (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMVideoForInterface < 500) )
      {
         g_uOSDDbmLastCaptureTime[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeVideo;
         g_fOSDDbm[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMVideoForInterface;
      }
      if ( 0 == g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeVideo )
      {
         if ( (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMDataForInterface > -500) && (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMDataForInterface < 500) )
         {
            g_uOSDDbmLastCaptureTime[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeData;
            g_fOSDDbm[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMDataForInterface;
         }
      }

      if ( (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRVideoForInterface > -500) && (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRVideoForInterface < 500) )
      {
         g_uOSDDbmLastCaptureTime[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeVideo;
         g_fOSDSNR[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRVideoForInterface;
      }
      if ( 0 == g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeVideo )
      {
         if ( (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRDataForInterface > -500) && (g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRDataForInterface < 500) )
         {
            g_uOSDDbmLastCaptureTime[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeData;
            g_fOSDSNR[i] = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRDataForInterface;
         }
      }
   }
}