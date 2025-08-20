#pragma once

#include "../base/base.h"

int negociate_radio_process_received_radio_link_messages(u8* pPacketBuffer);
void negociate_radio_periodic_loop();

bool negociate_radio_link_is_in_progress();
void negociate_radio_set_end_video_bitrate(u32 uVideoBitrateBPS);
u32 negociate_radio_link_get_radio_flags();
int negociate_radio_link_get_data_rate();
int negociate_radio_link_get_txpower_mw();


