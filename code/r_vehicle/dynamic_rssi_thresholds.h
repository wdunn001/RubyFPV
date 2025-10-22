#pragma once

#include "../base/base.h"

// MCS to RSSI threshold lookup table (based on 802.11n/ac standards)
extern const int g_MCS_RSSI_Thresholds[10];

// Dynamic RSSI threshold configuration
struct DynamicRSSIConfig {
    int hardware_rssi_offset;           // Hardware calibration offset (dBm)
    bool enable_dynamic_thresholds;     // Enable/disable feature
    int safety_margin_db;              // Safety margin for emergency drops (dBm)
};

// Function declarations
void dynamic_rssi_init();
void dynamic_rssi_save_config();
void dynamic_rssi_load_config();
int bitrate_to_mcs(int bitrate_mbps);
int get_dynamic_rssi_threshold(int current_mcs, int hardware_offset);
int get_effective_rssi_threshold(int current_bitrate, int static_threshold, int hardware_offset);
bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi, int static_threshold, int hardware_offset);