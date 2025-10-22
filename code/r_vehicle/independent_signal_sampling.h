#pragma once

#include "../base/base.h"
#include <time.h>

// Independent Signal Sampling Configuration
typedef struct {
    bool enable_independent_sampling;  // Enable/disable independent sampling
    int signal_sampling_freq_hz;       // Signal sampling frequency (Hz)
    int signal_sampling_interval;      // Legacy frame-based interval
    long signal_sampling_interval_ns;  // Nanosecond interval for precise timing
} signal_sampling_config_t;

// Signal Sampling State
typedef struct {
    struct timespec last_signal_time;   // Last signal sampling time
    bool new_signal_data;               // Flag indicating new signal data
    int sample_count;                   // Sample counter for statistics
    float actual_sampling_rate;          // Actual achieved sampling rate
} signal_sampling_state_t;

// Function declarations
void independent_signal_sampling_init();
void independent_signal_sampling_cleanup();

// Timing functions
bool should_sample_signal_now();
void update_signal_sampling_timing();
long get_signal_sampling_interval_ns();

// Configuration functions
void signal_sampling_save_config();
void signal_sampling_load_config();
void signal_sampling_set_frequency(int freq_hz);
int signal_sampling_get_frequency();

// Statistics functions
float get_actual_sampling_rate();
int get_sample_count();
void reset_sampling_statistics();

// Main sampling function
bool sample_signal_data(float* rssi, float* dbm, bool racing_mode);
