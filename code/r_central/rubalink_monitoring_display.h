#pragma once

#include "../../base/base.h"

// Real-time Monitoring Data Structure
typedef struct {
    // Current Status
    int current_mcs;
    int current_qp_delta;
    int effective_rssi_threshold;
    float signal_sampling_rate;
    
    // Signal Processing Status
    bool advanced_signal_processing_active;
    int rssi_filter_chain_active;
    int dbm_filter_chain_active;
    float filtered_rssi;
    float filtered_dbm;
    
    // Cooldown Status
    bool enhanced_cooldown_active;
    int control_algorithm_active;
    unsigned long last_bitrate_change_time;
    int current_bitrate;
    int target_bitrate;
    
    // Racing Mode Status
    bool racing_mode_active;
    int racing_fps;
    char racing_video_resolution[64];
    int racing_exposure;
    
    // Performance Metrics
    int bitrate_changes_per_minute;
    float average_signal_stability;
    int emergency_drops_count;
    unsigned long uptime_ms;
} rubalink_monitoring_data_t;

// Function declarations
void rubalink_monitoring_display_init();
void rubalink_monitoring_display_cleanup();
void rubalink_monitoring_display_show();
void rubalink_monitoring_display_hide();
bool rubalink_monitoring_display_is_active();

// Data update functions
void rubalink_monitoring_update_data();
void rubalink_monitoring_update_signal_processing_status();
void rubalink_monitoring_update_cooldown_status();
void rubalink_monitoring_update_racing_mode_status();
void rubalink_monitoring_update_performance_metrics();

// Display functions
void rubalink_monitoring_render();
void rubalink_monitoring_render_status_overlay();
void rubalink_monitoring_render_signal_processing_info();
void rubalink_monitoring_render_cooldown_info();
void rubalink_monitoring_render_racing_mode_info();
void rubalink_monitoring_render_performance_info();

// Data access functions
rubalink_monitoring_data_t* rubalink_monitoring_get_data();
void rubalink_monitoring_set_data(const rubalink_monitoring_data_t* data);

// Statistics functions
void rubalink_monitoring_reset_statistics();
void rubalink_monitoring_get_statistics(char* stats_buffer, int max_length);
