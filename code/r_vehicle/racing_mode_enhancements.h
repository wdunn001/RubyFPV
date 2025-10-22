#pragma once

#include "../base/base.h"

// Racing Mode Configuration
typedef struct {
    bool race_mode_enabled;              // Enable/disable racing mode
    int racing_fps;                      // Racing frame rate
    char racing_video_resolution[64];    // Racing video resolution
    int racing_exposure;                 // Racing exposure setting
    int racing_bitrate_max;              // Maximum bitrate in racing mode
    
    // Racing-specific filter chains
    char racing_rssi_filter_chain[64];   // Racing RSSI filter chain
    char racing_dbm_filter_chain[64];   // Racing dBm filter chain
    
    // Racing-specific cooldown settings
    unsigned long racing_strict_cooldown_ms;
    unsigned long racing_up_cooldown_ms;
    unsigned long racing_emergency_cooldown_ms;
    
    // Racing-specific QP delta settings
    int racing_qp_delta_low;
    int racing_qp_delta_medium;
    int racing_qp_delta_high;
} racing_mode_config_t;

// Racing Mode State
typedef struct {
    bool racing_mode_active;             // Currently in racing mode
    bool racing_mode_transitioning;      // Transitioning to/from racing mode
    int original_fps;                    // Original FPS before racing mode
    char original_resolution[64];        // Original resolution before racing mode
    int original_exposure;               // Original exposure before racing mode
    int original_bitrate_max;            // Original max bitrate before racing mode
} racing_mode_state_t;

// Function declarations
void racing_mode_init();
void racing_mode_cleanup();

// Racing mode control functions
void enable_racing_mode();
void disable_racing_mode();
bool is_racing_mode_enabled();
bool is_racing_mode_active();

// Configuration functions
void racing_mode_save_config();
void racing_mode_load_config();
void racing_mode_set_preset(const char* preset_name);

// Racing-specific processing functions
float process_racing_rssi_signal(float raw_rssi);
float process_racing_dbm_signal(float raw_dbm);
int calculate_racing_bitrate_change(int target_bitrate, int current_bitrate, float filtered_rssi, float filtered_dbm);

// Racing mode transitions
void transition_to_racing_mode();
void transition_from_racing_mode();
void apply_racing_video_settings();
void restore_original_video_settings();

// Racing mode statistics
int get_racing_mode_duration_ms();
int get_racing_mode_transitions();
void reset_racing_mode_statistics();

// RubyFPV integration functions
int get_rubyfpv_current_fps();
const char* get_rubyfpv_current_resolution();
int get_rubyfpv_current_exposure();
int get_rubyfpv_current_bitrate_max();
void set_rubyfpv_settings_callbacks(int (*fps_callback)(), const char* (*resolution_callback)(), 
                                   int (*exposure_callback)(), int (*bitrate_callback)());
