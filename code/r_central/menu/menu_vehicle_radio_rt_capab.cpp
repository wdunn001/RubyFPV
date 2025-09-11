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

#include "menu.h"
#include "menu_vehicle_radio_rt_capab.h"
#include "menu_confirmation.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "../../base/tx_powers.h"


MenuVehicleRadioRuntimeCapabilities::MenuVehicleRadioRuntimeCapabilities(void)
:Menu(MENU_ID_VEHICLE_RADIO_RUNTIME_CAPS, "Developer: Radio Runtime Capabilities", NULL)
{
   m_Width = 0.6;
   m_Height = 0.64;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;

   addMenuItem(new MenuItem(L("Ok"), L("Close this menu.")));
}

MenuVehicleRadioRuntimeCapabilities::~MenuVehicleRadioRuntimeCapabilities()
{
}

void MenuVehicleRadioRuntimeCapabilities::onShow()
{
   valuesToUI();
   Menu::onShow();
   invalidate();
}

void MenuVehicleRadioRuntimeCapabilities::valuesToUI()
{
   
}

void MenuVehicleRadioRuntimeCapabilities::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   RenderEnd(yTop);

   // Render data
   char szBuff[128];
   float xPos = m_xPos+m_sfMenuPaddingX;
   float yPos = yTop;
   float width = m_RenderWidth-2*m_sfMenuPaddingX;

   float xPosCol2 = xPos + width*0.44 + 2.0*Menu::getMenuPaddingX();
   float xPosCol3 = xPos + width*0.88 + 2.0*Menu::getMenuPaddingX();
   float xEndCol1 = xPosCol2 - 2.0*Menu::getMenuPaddingX();
   float xEndCol2 = xPosCol3 - 2.0*Menu::getMenuPaddingX();

   int iFontId = g_idFontMenu;
   float height_text_font = g_pRenderEngine->textHeight(iFontId);

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[0];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;
   int iRadioInterfacePowerMaxMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);

   g_pRenderEngine->setColors(get_Color_MenuText());
   sprintf(szBuff, "Max radio Tx power for vehicle radio link %d: %d mW", 1, iRadioInterfacePowerMaxMw );
   g_pRenderEngine->drawText(xPos, yPos, iFontId, szBuff);
   yPos += 1.4*height_text_font;
   yTop = yPos;

   g_pRenderEngine->drawText(xPos, yPos, iFontId, "Legacy datarates quality/max power:");
   g_pRenderEngine->drawText(xPosCol2, yPos, iFontId, "MCS datarates quality/max power:");
   yPos += 1.4*height_text_font;

   int iCountRates = getTestDataRatesCountLegacy();
   if ( getTestDataRatesCountMCS() > iCountRates )
      iCountRates = getTestDataRatesCountMCS();

   int iIndexMaxLegacy = -1;
   int iIndexMaxMCS = -1;
   float fMaxQLegacy = 0.0;
   float fMaxQMCS = 0.0;

   for( int i=0; i<iCountRates; i++ )
   {
      if ( i < getTestDataRatesCountLegacy() )
      if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[0][i] > fMaxQLegacy )
      {
         fMaxQLegacy = g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[0][i];
         iIndexMaxLegacy = i;
      }
      if ( i < getTestDataRatesCountMCS() )
      if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[0][i] > fMaxQMCS )
      {
         fMaxQMCS = g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[0][i];
         iIndexMaxMCS = i;
      }
   }

   if ( ! (g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
   {
      yPos += 0.5*height_text_font;
      g_pRenderEngine->drawText(xPos, yPos, iFontId, "NOT FULLY COMPUTED");
      g_pRenderEngine->drawText(xPosCol2, yPos, iFontId, "NOT FULLY COMPUTED");
      yPos += 1.5*height_text_font;
   }

   sprintf(szBuff, "Max usable rate: %s", str_format_datarate_inline(g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate));
   g_pRenderEngine->drawText(xPos, yPos, iFontId, szBuff);
   sprintf(szBuff, "Max usable rate: %s", str_format_datarate_inline(g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate));
   g_pRenderEngine->drawText(xPosCol2, yPos, iFontId, szBuff);
   yPos += 1.5*height_text_font;

   for( int i=0; i<iCountRates; i++ )
   {
      char szLegacy[64];
      char szMCS[64];
      char szLegacyQ[64];
      char szMCSQ[64];
      char szLegacyPower[64];
      char szMCSPower[64];

      szLegacy[0] = 0;
      szLegacyQ[0] = 0;
      szLegacyPower[0] = 0;
      szMCS[0] = 0;
      szMCSQ[0] = 0;
      szMCSPower[0] = 0;

      bool bShowRedPL = false;
      bool bShowRedPM = false;
      bool bShowYellowPL = false;
      bool bShowYellowPM = false;

      bool bShowRedL = false;
      bool bShowRedM = false;
      bool bShowYellowL = false;
      bool bShowYellowM = false;

      if ( i < getTestDataRatesCountLegacy() )
      {
         sprintf(szLegacy, "%u Mb:", getTestDataRatesLegacy()[i]/1000/1000);
         sprintf(szLegacyQ, "%d%%", (int)(100.0*g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[0][i]));
         sprintf(szLegacyPower, "%d mW", g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[0][i]);

         if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[0][i] < 0.2 )
         {
            bShowRedL = true;
            strcpy(szLegacyQ, "---");
         }
         else if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[0][i] < 0.95 )
            bShowYellowL = true;

         if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[0][i] <= 0 )
         {
            bShowRedPL = true;
            strcpy(szLegacyPower, "-- mW");
         }
         else if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[0][i] < iRadioInterfacePowerMaxMw*0.96 )
            bShowYellowPL = true;

      }
      else
      {
         szLegacy[0] = 0;
         szLegacyQ[0] = 0;
         szLegacyPower[0] = 0;
      }
      if ( i < getTestDataRatesCountMCS() )
      {
         sprintf(szMCS, "MCS-%d:", -1-getTestDataRatesMCS()[i]);
         sprintf(szMCSQ, "%d%%", (int)(100.0*g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[0][i]));
         sprintf(szMCSPower, "%d mW", g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[0][i]);

         if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[0][i] < 0.2 )
         {
            bShowRedM = true;
            strcpy(szMCSQ, "---");
         }
         else if ( g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[0][i] < 0.95 )
            bShowYellowM = true;

         if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[0][i] <= 0 )
         {
            bShowRedPM = true;
            strcpy(szMCSPower, "-- mW");
         }
         else if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[0][i] < iRadioInterfacePowerMaxMw-1 )
            bShowYellowPM = true;
      }
      else
      {
         szMCS[0] = 0;
         szMCSQ[0] = 0;
         szMCSPower[0] = 0;
      }
      
      g_pRenderEngine->drawText(xPos, yPos, iFontId, szLegacy);

      if ( bShowRedPL )
         g_pRenderEngine->setColors(get_Color_IconError());
      else if ( bShowYellowPL )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      else
         g_pRenderEngine->setColors(get_Color_MenuText());
   
      float fwtext = g_pRenderEngine->textWidth(iFontId, szLegacyPower);
      g_pRenderEngine->drawTextLeft(xEndCol1, yPos, iFontId, szLegacyPower);
      g_pRenderEngine->setColors(get_Color_MenuText());

      if ( iIndexMaxLegacy == i )
         g_pRenderEngine->setColors(get_Color_IconSucces());
      else if ( bShowRedL )
         g_pRenderEngine->setColors(get_Color_IconError());
      else if ( bShowYellowL )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      else
         g_pRenderEngine->setColors(get_Color_MenuText());

      g_pRenderEngine->drawTextLeft(xEndCol1 - fwtext - height_text_font*0.5, yPos, iFontId, szLegacyQ);
      g_pRenderEngine->setColors(get_Color_MenuText());

      g_pRenderEngine->drawText(xPosCol2, yPos, iFontId, szMCS);

      if ( bShowRedPM )
         g_pRenderEngine->setColors(get_Color_IconError());
      else if ( bShowYellowPM )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      else
         g_pRenderEngine->setColors(get_Color_MenuText());
   
      fwtext = g_pRenderEngine->textWidth(iFontId, szMCSPower);
      g_pRenderEngine->drawTextLeft(xEndCol2, yPos, iFontId, szMCSPower);
      g_pRenderEngine->setColors(get_Color_MenuText());

      if ( iIndexMaxMCS == i )
         g_pRenderEngine->setColors(get_Color_IconSucces());
      else if ( bShowRedM )
         g_pRenderEngine->setColors(get_Color_IconError());
      else if ( bShowYellowM )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      else
         g_pRenderEngine->setColors(get_Color_MenuText());

      g_pRenderEngine->drawTextLeft(xEndCol2 - fwtext - height_text_font*0.5, yPos, iFontId, szMCSQ);
      g_pRenderEngine->setColors(get_Color_MenuText());

      yPos += 1.1*height_text_font;
   }
   g_pRenderEngine->drawLine(xPosCol2 - Menu::getMenuPaddingX(), yTop, xPosCol2 - Menu::getMenuPaddingX(), yPos);
   g_pRenderEngine->drawLine(xPosCol3 - Menu::getMenuPaddingX(), yTop, xPosCol3 - Menu::getMenuPaddingX(), yPos);
   yPos += 1.5*height_text_font;

   y = yPos;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
}


void MenuVehicleRadioRuntimeCapabilities::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   menu_stack_pop(0);
}
