#pragma once

#include "../../base/base.h"

// RubALink Settings Panel Structure
typedef struct {
    // Dynamic RSSI Settings
    bool dynamic_rssi_enabled;
    int hardware_rssi_offset;
    int safety_margin_db;
    
    // QP Delta Settings
    bool qp_delta_enabled;
    int qp_delta_low;
    int qp_delta_medium;
    int qp_delta_high;
    
    // Advanced Signal Processing Settings
    bool advanced_signal_processing_enabled;
    int rssi_filter_chain;
    int dbm_filter_chain;
    int racing_rssi_filter_chain;
    int racing_dbm_filter_chain;
    float lpf_cutoff_freq;
    float lpf_sample_freq;
    float kalman_rssi_process;
    float kalman_dbm_process;
    float kalman_rssi_measure;
    float kalman_dbm_measure;
    
    // Independent Signal Sampling Settings
    bool independent_sampling_enabled;
    int signal_sampling_freq_hz;
    int signal_sampling_interval;
    
    // Enhanced Cooldown & Control Settings
    bool enhanced_cooldown_enabled;
    int strict_cooldown_ms;
    int up_cooldown_ms;
    int emergency_cooldown_ms;
    int min_change_percent;
    int control_algorithm;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    
    // Racing Mode Settings
    bool racing_mode_enabled;
    int racing_fps;
    char racing_video_resolution[64];
    int racing_exposure;
    int racing_bitrate_max;
    
    // Seamless Integration Settings
    bool seamless_integration_enabled;
    bool auto_detection_enabled;
    bool detect_by_vehicle_name;
    bool detect_by_video_settings;
    bool detect_by_bitrate_patterns;
    bool detect_by_signal_patterns;
    bool fallback_to_rubyfpv;
    bool fallback_to_rubalink;
    int fallback_timeout_ms;
    bool allow_user_override;
    bool remember_user_choice;
} rubalink_settings_t;

// Function declarations
void rubalink_settings_panel_init();
void rubalink_settings_panel_cleanup();
void rubalink_settings_panel_show();
void rubalink_settings_panel_hide();
bool rubalink_settings_panel_is_active();

// Settings management
void rubalink_settings_load_defaults();
void rubalink_settings_load_from_config();
void rubalink_settings_save_to_config();
void rubalink_settings_apply_preset(const char* preset_name);
void rubalink_settings_reset_to_defaults();

// UI control functions
void rubalink_settings_panel_render();
void rubalink_settings_panel_handle_input(int key);
void rubalink_settings_panel_navigate_up();
void rubalink_settings_panel_navigate_down();
void rubalink_settings_panel_increase_value();
void rubalink_settings_panel_decrease_value();
void rubalink_settings_panel_toggle_boolean();

// Validation functions
bool rubalink_settings_validate();
void rubalink_settings_validate_and_correct();
const char* rubalink_settings_get_validation_error();

// Preset management
void rubalink_settings_create_preset(const char* preset_name);
void rubalink_settings_delete_preset(const char* preset_name);
void rubalink_settings_list_presets(char* preset_list, int max_length);
bool rubalink_settings_preset_exists(const char* preset_name);

// Status functions
rubalink_settings_t* rubalink_settings_get_current();
void rubalink_settings_set_current(const rubalink_settings_t* settings);
bool rubalink_settings_has_unsaved_changes();
void rubalink_settings_mark_saved();
