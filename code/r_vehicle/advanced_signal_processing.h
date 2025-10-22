#pragma once

#include "../base/base.h"
#include <stdint.h>
#include <stdbool.h>

// Filter Types (matching AP-ALINK-FPV)
typedef enum {
    FILTER_TYPE_KALMAN = 0,
    FILTER_TYPE_LOWPASS = 1,
    FILTER_TYPE_MODE = 2,
    FILTER_TYPE_DERIVATIVE = 3,
    FILTER_TYPE_2POLE_LPF = 4
} filter_type_t;

// Control Algorithm Types
typedef enum {
    CONTROL_ALGORITHM_PID = 0,      // PID controller (complex, smooth)
    CONTROL_ALGORITHM_FIFO = 1      // Simple FIFO (fast, direct)
} control_algorithm_t;

// Kalman Filter Structure
typedef struct {
    float estimate;           // Current estimate
    float error_estimate;     // Current error estimate
    float process_variance;   // Process noise variance
    float measurement_variance; // Measurement noise variance
} kalman_filter_t;

// ArduPilot-style Low-Pass Filter Structure
typedef struct {
    float output;           // Current filtered output
    float alpha;           // Filter coefficient (0-1)
    bool initialised;      // Initialization flag
    float cutoff_freq;     // Cutoff frequency (Hz)
    float sample_freq;     // Sample frequency (Hz)
} lowpass_filter_t;

// Mode Filter Structure (ArduPilot style - median-like with alternating drop)
#define MODE_FILTER_SIZE 5
typedef struct {
    float samples[MODE_FILTER_SIZE];
    uint8_t sample_index;
    uint8_t return_element;
    bool drop_high_sample;
    float output;
} mode_filter_t;

// Derivative Filter Structure (for trend detection)
#define DERIVATIVE_FILTER_SIZE 5
typedef struct {
    float samples[DERIVATIVE_FILTER_SIZE];
    uint32_t timestamps[DERIVATIVE_FILTER_SIZE];
    uint8_t sample_index;
    float last_slope;
    bool new_data;
} derivative_filter_t;

// 2-Pole Low-Pass Filter Structure (biquad filter)
typedef struct {
    float delay_element_1;
    float delay_element_2;
    float cutoff_freq;
    float sample_freq;
    bool initialised;
    float output;
} lpf_2pole_t;

// Filter Chain Structure - allows multiple filters in sequence
#define MAX_FILTERS_PER_CHAIN 3
typedef struct {
    filter_type_t filters[MAX_FILTERS_PER_CHAIN];
    uint8_t filter_count;
    bool enabled;
} filter_chain_t;

// PID Controller Structure
typedef struct {
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    float integral;     // Integral accumulator
    int last_error;     // Previous error for derivative calculation
    int last_output;    // Previous output for reference
} pid_controller_t;

// Advanced Signal Processing Configuration
typedef struct {
    // Filter chain configurations
    filter_chain_t rssi_filter_chain;
    filter_chain_t dbm_filter_chain;
    filter_chain_t racing_rssi_filter_chain;
    filter_chain_t racing_dbm_filter_chain;
    
    // Filter parameters
    float lpf_cutoff_freq;
    float lpf_sample_freq;
    float kalman_rssi_process;
    float kalman_dbm_process;
    float kalman_rssi_measure;
    float kalman_dbm_measure;
    
    // Control algorithm
    int control_algorithm;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    
    // Cooldown parameters
    unsigned long strict_cooldown_ms;
    unsigned long up_cooldown_ms;
    unsigned long emergency_cooldown_ms;
    int min_change_percent;
} advanced_signal_config_t;

// Function declarations
void advanced_signal_processing_init();
void advanced_signal_processing_cleanup();

// Filter functions
float kalman_filter_update(kalman_filter_t *filter, float measurement);
float lowpass_filter_apply(lowpass_filter_t *filter, float sample);
float mode_filter_apply(mode_filter_t *filter, float sample);
float derivative_filter_apply(derivative_filter_t *filter, float sample);
float lpf_2pole_apply(lpf_2pole_t *filter, float sample);

// Filter chain functions
float apply_filter_chain(filter_chain_t *chain, float sample);
void parse_filter_chain(const char *config_str, filter_chain_t *chain);
void reset_filter_chain(filter_chain_t *chain);

// PID controller functions
int pid_calculate(pid_controller_t *pid, int target, int current);
void pid_reset(pid_controller_t *pid);
void pid_init(pid_controller_t *pid, float kp, float ki, float kd);

// Configuration functions
void advanced_signal_save_config();
void advanced_signal_load_config();
void advanced_signal_set_preset(const char* preset_name);

// Main processing functions
float process_rssi_signal(float raw_rssi, bool racing_mode);
float process_dbm_signal(float raw_dbm, bool racing_mode);
int calculate_bitrate_adjustment(int target_bitrate, int current_bitrate, bool racing_mode);
