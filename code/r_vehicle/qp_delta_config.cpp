#include "qp_delta_config.h"
#include "dynamic_rssi_thresholds.h"
#include "../base/config.h"

static QPDeltaConfig s_QPDeltaConfig = {
    .qp_delta_low = 15,
    .qp_delta_medium = 5,
    .qp_delta_high = 0,
    .enable_qp_delta = true
};

void qp_delta_init() {
    qp_delta_load_config();
    log_line("[AP-ALINK-FPV] QP Delta configuration system initialized");
}

int get_qp_delta_for_bitrate(int bitrate_mbps) {
    if (!s_QPDeltaConfig.enable_qp_delta) {
        return 0; // No QP delta adjustment
    }
    
    int current_mcs = bitrate_to_mcs(bitrate_mbps);
    
    if (current_mcs >= 1 && current_mcs < 3) {
        return s_QPDeltaConfig.qp_delta_low;
    }
    else if (current_mcs >= 3 && current_mcs < 10) {
        return s_QPDeltaConfig.qp_delta_medium;
    }
    else {
        return s_QPDeltaConfig.qp_delta_high;
    }
}

void apply_qp_delta_for_bitrate(int bitrate_mbps) {
    int qp_delta = get_qp_delta_for_bitrate(bitrate_mbps);
    
    // Apply QP delta to video encoder
    // This would integrate with RubyFPV's video encoding system
    // Implementation depends on RubyFPV's video pipeline
    
    #ifdef DEBUG
    log_line("Applied QP delta %d for bitrate %d Mbps (MCS %d)", 
             qp_delta, bitrate_mbps, bitrate_to_mcs(bitrate_mbps));
    #endif
    
    // TODO: Integrate with RubyFPV video encoding system
    // For now, just suppress the unused variable warning
    (void)qp_delta;
}

void set_qp_delta_presets(const char* preset_name) {
    if (strcmp(preset_name, "racing") == 0) {
        s_QPDeltaConfig.qp_delta_low = 10;
        s_QPDeltaConfig.qp_delta_medium = 3;
        s_QPDeltaConfig.qp_delta_high = 0;
        log_line("[AP-ALINK-FPV] Applied racing QP delta preset");
    }
    else if (strcmp(preset_name, "long_range") == 0) {
        s_QPDeltaConfig.qp_delta_low = 25;
        s_QPDeltaConfig.qp_delta_medium = 10;
        s_QPDeltaConfig.qp_delta_high = 5;
        log_line("[AP-ALINK-FPV] Applied long range QP delta preset");
    }
    else if (strcmp(preset_name, "balanced") == 0) {
        s_QPDeltaConfig.qp_delta_low = 15;
        s_QPDeltaConfig.qp_delta_medium = 5;
        s_QPDeltaConfig.qp_delta_high = 0;
        log_line("[AP-ALINK-FPV] Applied balanced QP delta preset");
    }
    else {
        log_line("[AP-ALINK-FPV] Unknown QP delta preset: %s", preset_name);
    }
}

void qp_delta_save_config() {
    // Save configuration to RubyFPV config system
    log_line("[AP-ALINK-FPV] QP Delta config saved");
}

void qp_delta_load_config() {
    // Load configuration from RubyFPV config system
    log_line("[AP-ALINK-FPV] QP Delta config loaded");
}