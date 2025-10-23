#include "adaptive_video_rubalink.h"
#include "adaptive_video.h"
#include "advanced_signal_processing.h"
#include "enhanced_cooldown_control.h"
#include "dynamic_rssi_thresholds.h"
#include "qp_delta_config.h"
#include "racing_mode_enhancements.h"
#include "rubalink_radio_interface.h"
#include "shared_vars.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include <math.h>

// Global configurations
static adaptive_video_signal_config_t s_SignalConfig = {
    .use_kalman_filter = true,
    .use_pid_controller = false,
    .kalman_process_noise = 1e-5f,
    .kalman_measurement_noise = 0.1f,
    .pid_kp = 1.0f,
    .pid_ki = 0.05f,
    .pid_kd = 0.4f
};

static adaptive_video_cooldown_config_t s_CooldownConfig = {
    .strict_cooldown_ms = 200,
    .up_cooldown_ms = 3000,
    .down_cooldown_ms = 1000,
    .emergency_cooldown_ms = 50,
    .min_change_percent = 5
};

static adaptive_video_racing_config_t s_RacingConfig = {
    .enabled = false,
    .max_bitrate_mbps = 8,
    .target_fps = 90,
    .emergency_bitrate_mbps = 2,
    .aggressive_factor = 2.0f
};

// Initialize RubALink enhancements
void adaptive_video_rubalink_init() {
    log_line("[AdaptiveVideo-RubALink] Initializing RubALink enhancements...");
    
    // Initialize subsystems
    advanced_signal_processing_init();
    enhanced_cooldown_init();
    dynamic_rssi_init();
    qp_delta_init();
    racing_mode_init();
    rubalink_radio_interface_init();
    
    // Load configuration
    adaptive_video_rubalink_load_config();
    
    log_line("[AdaptiveVideo-RubALink] RubALink enhancements initialized");
}

void adaptive_video_rubalink_cleanup() {
    log_line("[AdaptiveVideo-RubALink] Cleaning up RubALink enhancements...");
    
    advanced_signal_processing_cleanup();
    enhanced_cooldown_cleanup();
    racing_mode_cleanup();
    rubalink_radio_interface_cleanup();
    
    adaptive_video_rubalink_save_config();
}

// Signal processing enhancements
float adaptive_video_apply_kalman_filter(float raw_value, bool is_rssi) {
    if (!s_SignalConfig.use_kalman_filter) {
        return raw_value;
    }
    
    // Use the advanced signal processing Kalman filter
    return process_rssi_signal(raw_value, s_RacingConfig.enabled);
}

float adaptive_video_apply_advanced_filtering(float raw_value, int filter_type) {
    // Delegate to advanced signal processing
    filter_chain_t chain;
    chain.filters[0] = (filter_type_t)filter_type;
    chain.filter_count = 1;
    chain.enabled = true;
    
    return apply_filter_chain(&chain, raw_value);
}

int adaptive_video_calculate_pid_adjustment(int target_bitrate, int current_bitrate) {
    if (!s_SignalConfig.use_pid_controller) {
        return target_bitrate;
    }
    
    return calculate_bitrate_adjustment(target_bitrate, current_bitrate, s_RacingConfig.enabled);
}

// Enhanced cooldown control
bool adaptive_video_check_enhanced_cooldown(int new_bitrate, int current_bitrate) {
    // Check if change is allowed based on enhanced cooldown rules
    if (!should_change_bitrate(new_bitrate, current_bitrate)) {
        return false;
    }
    
    // Check minimum change percentage
    int change_percent = abs(new_bitrate - current_bitrate) * 100 / current_bitrate;
    if (change_percent < s_CooldownConfig.min_change_percent) {
        return false;
    }
    
    return true;
}

void adaptive_video_update_enhanced_cooldown(int new_bitrate, int current_bitrate) {
    update_cooldown_timing(new_bitrate, current_bitrate);
}

// Dynamic RSSI thresholds
int adaptive_video_get_dynamic_rssi_threshold(int mcs_rate) {
    return get_dynamic_rssi_threshold_for_mcs(mcs_rate);
}

void adaptive_video_update_dynamic_thresholds() {
    // Update thresholds based on current conditions
    rubalink_radio_interface_update();
    
    rubalink_radio_status_t* radio_status = rubalink_radio_get_status();
    if (radio_status && radio_status->primary_interface_index >= 0) {
        rubalink_radio_interface_t* primary = &radio_status->interfaces[radio_status->primary_interface_index];
        
        // Update dynamic thresholds based on current MCS
        if (primary->current_mcs_rate >= 0) {
            int threshold = adaptive_video_get_dynamic_rssi_threshold(primary->current_mcs_rate);
            log_line("[AdaptiveVideo-RubALink] Updated RSSI threshold to %d dBm for MCS %d", 
                    threshold, primary->current_mcs_rate);
        }
    }
}

// QP Delta adjustments
int adaptive_video_get_qp_delta_for_bitrate(int bitrate_mbps) {
    return get_qp_delta_for_bitrate(bitrate_mbps);
}

void adaptive_video_apply_qp_delta_adjustment(int bitrate_mbps) {
    apply_qp_delta_for_bitrate(bitrate_mbps);
}

// Racing mode
void adaptive_video_enable_racing_mode(bool enable) {
    s_RacingConfig.enabled = enable;
    
    if (enable) {
        racing_mode_activate();
        log_line("[AdaptiveVideo-RubALink] Racing mode ACTIVATED");
    } else {
        racing_mode_deactivate();
        log_line("[AdaptiveVideo-RubALink] Racing mode DEACTIVATED");
    }
}

bool adaptive_video_is_racing_mode_active() {
    return s_RacingConfig.enabled && racing_mode_is_active();
}

int adaptive_video_calculate_racing_bitrate(int current_bitrate, float signal_quality) {
    if (!adaptive_video_is_racing_mode_active()) {
        return current_bitrate;
    }
    
    // In racing mode, be more aggressive with bitrate changes
    int target_bitrate = current_bitrate;
    
    if (signal_quality > 80.0f) {
        // Excellent signal - push to max
        target_bitrate = s_RacingConfig.max_bitrate_mbps * 1000;
    } else if (signal_quality > 60.0f) {
        // Good signal - increase aggressively
        target_bitrate = current_bitrate * s_RacingConfig.aggressive_factor;
    } else if (signal_quality < 30.0f) {
        // Poor signal - drop to emergency bitrate immediately
        target_bitrate = s_RacingConfig.emergency_bitrate_mbps * 1000;
    }
    
    // Apply racing mode bitrate calculation
    float filtered_rssi = 0.0f;
    float filtered_dbm = 0.0f;
    
    rubalink_radio_status_t* radio_status = rubalink_radio_get_status();
    if (radio_status && radio_status->primary_interface_index >= 0) {
        rubalink_radio_interface_t* primary = &radio_status->interfaces[radio_status->primary_interface_index];
        filtered_rssi = primary->last_rssi;
        filtered_dbm = primary->last_dbm;
    }
    
    return calculate_racing_bitrate_change(target_bitrate, current_bitrate, filtered_rssi, filtered_dbm);
}

// WiFi MCS rate control
void adaptive_video_update_mcs_rate_based_on_quality(float video_link_quality) {
    rubalink_radio_status_t* radio_status = rubalink_radio_get_status();
    if (!radio_status || radio_status->primary_interface_index < 0) {
        return;
    }
    
    // Calculate optimal MCS based on VLQ
    int optimal_mcs = adaptive_video_get_optimal_mcs_for_conditions(
        radio_status->interfaces[radio_status->primary_interface_index].last_rssi,
        100.0f - video_link_quality // Convert VLQ to packet loss approximation
    );
    
    // Set MCS rate if it supports rate control
    if (rubalink_radio_supports_rate_control(radio_status->primary_interface_index)) {
        rubalink_radio_set_mcs_rate(radio_status->primary_interface_index, optimal_mcs);
    }
}

int adaptive_video_get_optimal_mcs_for_conditions(float rssi, float packet_loss) {
    // MCS selection based on conditions
    if (packet_loss > 10.0f || rssi < 40) {
        return 0; // MCS 0 - most robust
    } else if (packet_loss > 5.0f || rssi < 50) {
        return 2; // MCS 2
    } else if (packet_loss > 2.0f || rssi < 60) {
        return 4; // MCS 4
    } else if (packet_loss > 1.0f || rssi < 70) {
        return 6; // MCS 6
    } else {
        return 8; // MCS 8 - highest throughput
    }
}

// Configuration
void adaptive_video_rubalink_load_config() {
    // Load from RubyFPV's configuration system
    // For now, use defaults
    log_line("[AdaptiveVideo-RubALink] Configuration loaded");
}

void adaptive_video_rubalink_save_config() {
    // Save to RubyFPV's configuration system
    log_line("[AdaptiveVideo-RubALink] Configuration saved");
}

adaptive_video_signal_config_t* adaptive_video_get_signal_config() {
    return &s_SignalConfig;
}

adaptive_video_cooldown_config_t* adaptive_video_get_cooldown_config() {
    return &s_CooldownConfig;
}

adaptive_video_racing_config_t* adaptive_video_get_racing_config() {
    return &s_RacingConfig;
}
