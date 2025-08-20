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

#include "../../base/hardware_i2c.h"
#include "menu.h"
#include "menu_vehicle_video_profile.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

MenuVehicleVideoProfileSelector::MenuVehicleVideoProfileSelector(void)
:Menu(MENU_ID_VEHICLE_VIDEO_PROFILE+10*1000, L("Select Video Profile"), NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   char szLegend[256];
   
   m_pItemsRadio[0] = new MenuItemRadio("", "");

   strcpy(szLegend, "Use this option to automatically adjust video parameters for better video quality;");
   m_pItemsRadio[0]->addSelection(L("High Quality"), szLegend);

   strcpy(szLegend, "Use this option to automatically adjust video parameters for video low latency;");
   m_pItemsRadio[0]->addSelection(L("High Performance"), szLegend);

   strcpy(szLegend, "Use this option to automatically adjust video parameters for better range in detriment of video quality and latency;");
   m_pItemsRadio[0]->addSelection(L("Long Range"), szLegend);

   strcpy(szLegend, "Use this option if you wish to manually modify the advanced video parameters;");
   m_pItemsRadio[0]->addSelection(L("User Defined"), szLegend);

   m_pItemsRadio[0]->setEnabled(true);
   m_IndexVideoProfile = addMenuItem(m_pItemsRadio[0]);
}

MenuVehicleVideoProfileSelector::~MenuVehicleVideoProfileSelector()
{
}

void MenuVehicleVideoProfileSelector::onShow()
{      
   Menu::onShow();
}

void MenuVehicleVideoProfileSelector::valuesToUI()
{
   int index = g_pCurrentModel->video_params.iCurrentVideoProfile;

   m_pItemsRadio[0]->setSelectedIndex(index);
   m_pItemsRadio[0]->setFocusedIndex(index);
}


void MenuVehicleVideoProfileSelector::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideoProfileSelector::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexVideoProfile == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 289 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      log_line("Selected option %d", m_pItemsRadio[0]->getFocusedIndex());
      m_pItemsRadio[0]->setSelectedIndex(m_pItemsRadio[0]->getFocusedIndex());

      int index = m_pItemsRadio[0]->getSelectedIndex();

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.iCurrentVideoProfile = index;

      if ( paramsNew.iCurrentVideoProfile == g_pCurrentModel->video_params.iCurrentVideoProfile )
         return;

      log_line("Sending to vehicle new user selected video link profile: %s", str_get_video_profile_name(paramsNew.iCurrentVideoProfile));
      
      send_pause_adaptive_to_router(5000);
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
         valuesToUI();
   }
}
