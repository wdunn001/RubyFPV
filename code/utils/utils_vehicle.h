#pragma once

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/models.h"

u32 vehicle_utils_getControllerId();

// To fix
//void video_overwrites_init(shared_mem_video_link_overwrites* pSMVLO, Model* pModel);
bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_process_stats* pProcessStats);

int get_vehicle_radio_link_current_tx_power_mw(Model* pModel, int iRadioLinkIndex);
void apply_vehicle_tx_power_levels(Model* pModel);
