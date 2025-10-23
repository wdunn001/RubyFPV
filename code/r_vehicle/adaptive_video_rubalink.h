#pragma once

#include "../base/base.h"
#include "../base/hardware_radio.h"

// RubALink Enhancements for RubyFPV's Adaptive Video System
// This integrates unique RubALink features into the existing adaptive video

// Advanced signal filtering configuration
typedef struct {
    bool use_kalman_filter;
    bool use_pid_controller;
    float kalman_process_noise;
    float kalman_measurement_noise;
    float pid_kp;
    float pid_ki; 
    float pid_kd;
} adaptive_video_signal_config_t;

// Enhanced cooldown configuration
typedef struct {
    unsigned long strict_cooldown_ms;
    unsigned long up_cooldown_ms;
    unsigned long down_cooldown_ms;
    unsigned long emergency_cooldown_ms;
    int min_change_percent;
} adaptive_video_cooldown_config_t;

// Racing mode configuration
typedef struct {
    bool enabled;
    int max_bitrate_mbps;
    int target_fps;
    int emergency_bitrate_mbps;
    float aggressive_factor;
} adaptive_video_racing_config_t;

// Initialize RubALink enhancements for adaptive video
void adaptive_video_rubalink_init();
void adaptive_video_rubalink_cleanup();

// Signal processing enhancements
float adaptive_video_apply_kalman_filter(float raw_value, bool is_rssi);
float adaptive_video_apply_advanced_filtering(float raw_value, int filter_type);
int adaptive_video_calculate_pid_adjustment(int target_bitrate, int current_bitrate);

// Enhanced cooldown control
bool adaptive_video_check_enhanced_cooldown(int new_bitrate, int current_bitrate);
void adaptive_video_update_enhanced_cooldown(int new_bitrate, int current_bitrate);

// Dynamic RSSI thresholds based on MCS
int adaptive_video_get_dynamic_rssi_threshold(int mcs_rate);
void adaptive_video_update_dynamic_thresholds();

// QP Delta adjustments for OpenIPC
int adaptive_video_get_qp_delta_for_bitrate(int bitrate_mbps);
void adaptive_video_apply_qp_delta_adjustment(int bitrate_mbps);

// Racing mode enhancements
void adaptive_video_enable_racing_mode(bool enable);
bool adaptive_video_is_racing_mode_active();
int adaptive_video_calculate_racing_bitrate(int current_bitrate, float signal_quality);

// WiFi MCS rate control integration
void adaptive_video_update_mcs_rate_based_on_quality(float video_link_quality);
int adaptive_video_get_optimal_mcs_for_conditions(float rssi, float packet_loss);

// Configuration
void adaptive_video_rubalink_load_config();
void adaptive_video_rubalink_save_config();
adaptive_video_signal_config_t* adaptive_video_get_signal_config();
adaptive_video_cooldown_config_t* adaptive_video_get_cooldown_config();
adaptive_video_racing_config_t* adaptive_video_get_racing_config();
