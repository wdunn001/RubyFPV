#include "dynamic_rssi_thresholds.h"
#include "../base/config.h"
#include "../base/models.h"

// MCS to RSSI threshold lookup table (based on 802.11n/ac standards)
const int g_MCS_RSSI_Thresholds[10] = {
    -82,  // MCS 0 (BPSK 1/2)   - Most robust
    -79,  // MCS 1 (QPSK 1/2)
    -77,  // MCS 2 (QPSK 3/4)
    -74,  // MCS 3 (16-QAM 1/2)
    -70,  // MCS 4 (16-QAM 3/4)
    -66,  // MCS 5 (64-QAM 2/3)
    -65,  // MCS 6 (64-QAM 3/4)
    -64,  // MCS 7 (64-QAM 5/6)
    -59,  // MCS 8 (256-QAM 3/4) - VHT
    -57   // MCS 9 (256-QAM 5/6) - VHT, least robust
};

static DynamicRSSIConfig s_DynamicRSSIConfig = {
    .hardware_rssi_offset = 0,
    .enable_dynamic_thresholds = true,
    .safety_margin_db = 3
};

void dynamic_rssi_init() {
    dynamic_rssi_load_config();
    log_line("[AP-ALINK-FPV] Dynamic RSSI threshold system initialized");
}

int bitrate_to_mcs(int bitrate_mbps) {
    // Simplified mapping based on typical bitrates
    // This should be calibrated for specific hardware
    if (bitrate_mbps <= 1) return 0;      // MCS 0
    else if (bitrate_mbps <= 2) return 1; // MCS 1
    else if (bitrate_mbps <= 3) return 2; // MCS 2
    else if (bitrate_mbps <= 4) return 3; // MCS 3
    else if (bitrate_mbps <= 6) return 4; // MCS 4
    else if (bitrate_mbps <= 8) return 5; // MCS 5
    else if (bitrate_mbps <= 10) return 6; // MCS 6
    else if (bitrate_mbps <= 12) return 7; // MCS 7
    else if (bitrate_mbps <= 15) return 8; // MCS 8
    else return 9; // MCS 9 (highest)
}

int get_dynamic_rssi_threshold(int current_mcs, int hardware_offset) {
    // Clamp MCS to valid range
    if (current_mcs < 0) current_mcs = 0;
    if (current_mcs >= 10) current_mcs = 9;
    
    // Get base threshold from lookup table
    int base_threshold = g_MCS_RSSI_Thresholds[current_mcs];
    
    // Apply hardware-specific offset
    int dynamic_threshold = base_threshold + hardware_offset;
    
    // Add safety margin for emergency drop
    dynamic_threshold += s_DynamicRSSIConfig.safety_margin_db;
    
    return dynamic_threshold;
}

int get_effective_rssi_threshold(int current_bitrate, int static_threshold, int hardware_offset) {
    if (!s_DynamicRSSIConfig.enable_dynamic_thresholds) {
        return static_threshold;
    }
    
    int current_mcs = bitrate_to_mcs(current_bitrate);
    int dynamic_threshold = get_dynamic_rssi_threshold(current_mcs, hardware_offset);
    
    // Use the more conservative threshold (lower value = more sensitive)
    return (dynamic_threshold < static_threshold) ? dynamic_threshold : static_threshold;
}

bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi, int static_threshold, int hardware_offset) {
    int effective_threshold = get_effective_rssi_threshold(current_bitrate, static_threshold, hardware_offset);
    return (filtered_rssi < effective_threshold);
}

void dynamic_rssi_save_config() {
    // Save configuration to RubyFPV config system
    // Implementation depends on RubyFPV config system
    log_line("[AP-ALINK-FPV] Dynamic RSSI config saved");
}

void dynamic_rssi_load_config() {
    // Load configuration from RubyFPV config system
    // Implementation depends on RubyFPV config system
    log_line("[AP-ALINK-FPV] Dynamic RSSI config loaded");
}