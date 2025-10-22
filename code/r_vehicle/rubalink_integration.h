#pragma once

#include "../base/base.h"

// RubALink Integration Status
typedef struct {
    bool dynamic_rssi_enabled;
    bool qp_delta_enabled;
    bool robust_commands_enabled;
    bool advanced_signal_processing_enabled;
    bool independent_sampling_enabled;
    bool enhanced_cooldown_enabled;
    bool racing_mode_enabled;
    bool seamless_integration_enabled;
    int current_mcs;
    int current_qp_delta;
    int effective_rssi_threshold;
} rubalink_integration_status_t;

// Function declarations
void rubalink_integration_init();
void rubalink_integration_cleanup();
void rubalink_adaptive_video_init();
void rubalink_adaptive_video_cleanup();

// Status functions
rubalink_integration_status_t rubalink_get_integration_status();
bool rubalink_is_integration_active();
void rubalink_set_integration_status(const rubalink_integration_status_t* status);

// Core system functions
void rubalink_process_adaptive_video();
void rubalink_update_signal_processing();
void rubalink_handle_bitrate_changes();
void rubalink_handle_emergency_conditions();

// Configuration functions
void rubalink_load_configuration();
void rubalink_save_configuration();
void rubalink_reset_to_defaults();
void rubalink_apply_preset(const char* preset_name);

// Integration with RubyFPV
void rubalink_integrate_with_rubyfpv_adaptive();
void rubalink_seamless_switch_systems();
bool rubalink_should_use_advanced_features();
bool rubalink_should_use_rubyfpv_adaptive();
