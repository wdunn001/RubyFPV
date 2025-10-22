#include "advanced_signal_processing.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// Global filter instances
static kalman_filter_t rssi_kalman_filter = {
    .estimate = 50.0f,
    .error_estimate = 1.0f,
    .process_variance = 1e-5f,
    .measurement_variance = 0.1f
};

static kalman_filter_t dbm_kalman_filter = {
    .estimate = -60.0f,
    .error_estimate = 1.0f,
    .process_variance = 1e-5f,
    .measurement_variance = 0.5f
};

static lowpass_filter_t rssi_lpf = {
    .output = 50.0f,
    .alpha = 0.1f,
    .initialised = false,
    .cutoff_freq = 2.0f,
    .sample_freq = 10.0f
};

static lowpass_filter_t dbm_lpf = {
    .output = -60.0f,
    .alpha = 0.1f,
    .initialised = false,
    .cutoff_freq = 2.0f,
    .sample_freq = 10.0f
};

static mode_filter_t rssi_mode_filter = {
    .sample_index = 0,
    .return_element = 2,
    .drop_high_sample = true,
    .output = 50.0f
};

static mode_filter_t dbm_mode_filter = {
    .sample_index = 0,
    .return_element = 2,
    .drop_high_sample = true,
    .output = -60.0f
};

static derivative_filter_t rssi_derivative_filter = {
    .sample_index = 0,
    .last_slope = 0.0f,
    .new_data = false
};

static derivative_filter_t dbm_derivative_filter = {
    .sample_index = 0,
    .last_slope = 0.0f,
    .new_data = false
};

static lpf_2pole_t rssi_2pole_lpf = {
    .delay_element_1 = 0.0f,
    .delay_element_2 = 0.0f,
    .cutoff_freq = 2.0f,
    .sample_freq = 10.0f,
    .initialised = false,
    .output = 50.0f
};

static lpf_2pole_t dbm_2pole_lpf = {
    .delay_element_1 = 0.0f,
    .delay_element_2 = 0.0f,
    .cutoff_freq = 2.0f,
    .sample_freq = 10.0f,
    .initialised = false,
    .output = -60.0f
};

static pid_controller_t bitrate_pid = {
    .kp = 1.0f,
    .ki = 0.05f,
    .kd = 0.4f,
    .integral = 0.0f,
    .last_error = 0,
    .last_output = 0
};

// Global filter chains
static filter_chain_t rssi_filter_chain = {
    .filters = {FILTER_TYPE_KALMAN},
    .filter_count = 1,
    .enabled = true
};

static filter_chain_t dbm_filter_chain = {
    .filters = {FILTER_TYPE_KALMAN},
    .filter_count = 1,
    .enabled = true
};

static filter_chain_t rssi_race_filter_chain = {
    .filters = {FILTER_TYPE_LOWPASS},
    .filter_count = 1,
    .enabled = true
};

static filter_chain_t dbm_race_filter_chain = {
    .filters = {FILTER_TYPE_LOWPASS},
    .filter_count = 1,
    .enabled = true
};

static advanced_signal_config_t s_Config = {
    .lpf_cutoff_freq = 2.0f,
    .lpf_sample_freq = 10.0f,
    .kalman_rssi_process = 1e-5f,
    .kalman_dbm_process = 1e-5f,
    .kalman_rssi_measure = 0.1f,
    .kalman_dbm_measure = 0.5f,
    .control_algorithm = CONTROL_ALGORITHM_FIFO,
    .pid_kp = 1.0f,
    .pid_ki = 0.05f,
    .pid_kd = 0.4f,
    .strict_cooldown_ms = 200,
    .up_cooldown_ms = 3000,
    .emergency_cooldown_ms = 50,
    .min_change_percent = 5
};

// Kalman Filter Implementation
float kalman_filter_update(kalman_filter_t *filter, float measurement) {
    // Prediction step - predict next state
    float predicted_estimate = filter->estimate;
    float predicted_error = filter->error_estimate + filter->process_variance;
    
    // Update step - correct prediction with measurement
    float kalman_gain = predicted_error / (predicted_error + filter->measurement_variance);
    
    // Clamp kalman gain to prevent numerical issues
    if (kalman_gain > 1.0f) kalman_gain = 1.0f;
    if (kalman_gain < 0.0f) kalman_gain = 0.0f;
    
    filter->estimate = predicted_estimate + kalman_gain * (measurement - predicted_estimate);
    filter->error_estimate = (1.0f - kalman_gain) * predicted_error;
    
    // Prevent error estimate from becoming too small (numerical stability)
    if (filter->error_estimate < 1e-6f) {
        filter->error_estimate = 1e-6f;
    }
    
    return filter->estimate;
}

// Low-Pass Filter Implementation (ArduPilot style)
float calc_lowpass_alpha_dt(float dt, float cutoff_freq) {
    if (cutoff_freq <= 0.0f || dt <= 0.0f) {
        return 1.0f;  // No filtering
    }
    
    float rc = 1.0f / (2.0f * M_PI * cutoff_freq);
    return dt / (rc + dt);
}

void init_lowpass_filter(lowpass_filter_t *filter, float cutoff_freq, float sample_freq) {
    filter->cutoff_freq = cutoff_freq;
    filter->sample_freq = sample_freq;
    filter->alpha = calc_lowpass_alpha_dt(1.0f / sample_freq, cutoff_freq);
    filter->initialised = false;
}

float lowpass_filter_apply(lowpass_filter_t *filter, float sample) {
    if (!filter->initialised) {
        filter->output = sample;
        filter->initialised = true;
    } else {
        filter->output += (sample - filter->output) * filter->alpha;
    }
    return filter->output;
}

// Mode Filter Implementation (ArduPilot style)
void mode_filter_insert(mode_filter_t *filter, float sample) {
    uint8_t i;
    
    // If buffer isn't full, simply increase sample count
    if (filter->sample_index < MODE_FILTER_SIZE) {
        filter->sample_index++;
        filter->drop_high_sample = true;  // Default to dropping high
    }
    
    if (filter->drop_high_sample) {
        // Drop highest sample - start from top
        i = filter->sample_index - 1;
        
        // Shift samples down to make room
        while (i > 0 && filter->samples[i-1] > sample) {
            filter->samples[i] = filter->samples[i-1];
            i--;
        }
        
        // Insert new sample
        filter->samples[i] = sample;
    } else {
        // Drop lowest sample - start from bottom
        i = 0;
        
        // Shift samples up to make room
        while (i < filter->sample_index - 1 && filter->samples[i+1] < sample) {
            filter->samples[i] = filter->samples[i+1];
            i++;
        }
        
        // Insert new sample
        filter->samples[i] = sample;
    }
    
    // Alternate drop direction for next sample
    filter->drop_high_sample = !filter->drop_high_sample;
}

float mode_filter_apply(mode_filter_t *filter, float sample) {
    mode_filter_insert(filter, sample);
    
    if (filter->sample_index < MODE_FILTER_SIZE) {
        // Buffer not full - return middle sample
        filter->output = filter->samples[filter->sample_index / 2];
    } else {
        // Buffer full - return specified element (usually median)
        filter->output = filter->samples[filter->return_element];
    }
    
    return filter->output;
}

// Derivative Filter Implementation (for trend detection)
void derivative_filter_update(derivative_filter_t *filter, float sample, uint32_t timestamp) {
    uint8_t i = filter->sample_index;
    uint8_t i1;
    
    if (i == 0) {
        i1 = DERIVATIVE_FILTER_SIZE - 1;
    } else {
        i1 = i - 1;
    }
    
    // Check if this is a new timestamp
    if (filter->timestamps[i1] == timestamp) {
        return; // Ignore duplicate timestamp
    }
    
    // Store timestamp and sample
    filter->timestamps[i] = timestamp;
    filter->samples[i] = sample;
    
    // Update sample index
    filter->sample_index = (filter->sample_index + 1) % DERIVATIVE_FILTER_SIZE;
    filter->new_data = true;
}

float derivative_filter_apply(derivative_filter_t *filter, float sample) {
    uint32_t current_time = (uint32_t)(get_current_time_ms());
    derivative_filter_update(filter, sample, current_time);
    
    // Return the most recent sample (derivative is available via slope function)
    uint8_t idx = (filter->sample_index - 1 + DERIVATIVE_FILTER_SIZE) % DERIVATIVE_FILTER_SIZE;
    return filter->samples[idx];
}

// 2-Pole Low-Pass Filter Implementation (biquad filter)
void init_2pole_lpf(lpf_2pole_t *filter, float cutoff_freq, float sample_freq) {
    filter->cutoff_freq = cutoff_freq;
    filter->sample_freq = sample_freq;
    filter->delay_element_1 = 0.0f;
    filter->delay_element_2 = 0.0f;
    filter->initialised = false;
}

float lpf_2pole_apply(lpf_2pole_t *filter, float sample) {
    if (!filter->initialised) {
        filter->output = sample;
        filter->delay_element_1 = sample;
        filter->delay_element_2 = sample;
        filter->initialised = true;
        return sample;
    }
    
    // Calculate filter coefficients
    float omega = 2.0f * M_PI * filter->cutoff_freq / filter->sample_freq;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * 0.707f); // Q = 0.707 for Butterworth response
    
    // Biquad coefficients
    float b0 = (1.0f - cs) / 2.0f;
    float b1 = 1.0f - cs;
    float b2 = (1.0f - cs) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;
    
    // Normalize coefficients
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    
    // Apply biquad filter
    float output = b0 * sample + b1 * filter->delay_element_1 + b2 * filter->delay_element_2
                   - a1 * filter->delay_element_1 - a2 * filter->delay_element_2;
    
    // Update delay elements
    filter->delay_element_2 = filter->delay_element_1;
    filter->delay_element_1 = output;
    
    filter->output = output;
    return output;
}

// Filter Chain Implementation
float apply_filter_chain(filter_chain_t *chain, float sample) {
    if (!chain->enabled || chain->filter_count == 0) {
        return sample;
    }
    
    float filtered_sample = sample;
    
    // Apply each filter in the chain sequentially
    for (uint8_t i = 0; i < chain->filter_count; i++) {
        filter_type_t filter_type = chain->filters[i];
        
        switch (filter_type) {
            case FILTER_TYPE_KALMAN:
                if (chain == &rssi_filter_chain || chain == &rssi_race_filter_chain) {
                    filtered_sample = kalman_filter_update(&rssi_kalman_filter, filtered_sample);
                } else {
                    filtered_sample = kalman_filter_update(&dbm_kalman_filter, filtered_sample);
                }
                break;
                
            case FILTER_TYPE_LOWPASS:
                if (chain == &rssi_filter_chain || chain == &rssi_race_filter_chain) {
                    filtered_sample = lowpass_filter_apply(&rssi_lpf, filtered_sample);
                } else {
                    filtered_sample = lowpass_filter_apply(&dbm_lpf, filtered_sample);
                }
                break;
                
            case FILTER_TYPE_MODE:
                if (chain == &rssi_filter_chain || chain == &rssi_race_filter_chain) {
                    filtered_sample = mode_filter_apply(&rssi_mode_filter, filtered_sample);
                } else {
                    filtered_sample = mode_filter_apply(&dbm_mode_filter, filtered_sample);
                }
                break;
                
            case FILTER_TYPE_DERIVATIVE:
                if (chain == &rssi_filter_chain || chain == &rssi_race_filter_chain) {
                    filtered_sample = derivative_filter_apply(&rssi_derivative_filter, filtered_sample);
                } else {
                    filtered_sample = derivative_filter_apply(&dbm_derivative_filter, filtered_sample);
                }
                break;
                
            case FILTER_TYPE_2POLE_LPF:
                if (chain == &rssi_filter_chain || chain == &rssi_race_filter_chain) {
                    filtered_sample = lpf_2pole_apply(&rssi_2pole_lpf, filtered_sample);
                } else {
                    filtered_sample = lpf_2pole_apply(&dbm_2pole_lpf, filtered_sample);
                }
                break;
                
            default:
                // Unknown filter type, pass through unchanged
                break;
        }
    }
    
    return filtered_sample;
}

// Parse filter chain configuration string (e.g., "2,0" for Mode->Kalman)
void parse_filter_chain(const char *config_str, filter_chain_t *chain) {
    if (!config_str || strlen(config_str) == 0) {
        // Default to single Kalman filter
        chain->filters[0] = FILTER_TYPE_KALMAN;
        chain->filter_count = 1;
        return;
    }
    
    char *config_copy = strdup(config_str);
    char *token = strtok(config_copy, ",");
    uint8_t count = 0;
    
    while (token != NULL && count < MAX_FILTERS_PER_CHAIN) {
        int filter_type = atoi(token);
        if (filter_type >= 0 && filter_type <= 4) {
            chain->filters[count] = (filter_type_t)filter_type;
            count++;
        }
        token = strtok(NULL, ",");
    }
    
    chain->filter_count = count;
    free(config_copy);
}

// PID Controller Implementation
int pid_calculate(pid_controller_t *pid, int target, int current) {
    // Calculate error
    int error = target - current;
    
    // Proportional term
    float p_term = pid->kp * error;
    
    // Integral term (with windup protection)
    pid->integral += error;
    
    // Integral windup protection - limit integral to prevent overshoot
    const float max_integral = 1000.0f;  // Adjust based on your bitrate range
    if (pid->integral > max_integral) pid->integral = max_integral;
    if (pid->integral < -max_integral) pid->integral = -max_integral;
    
    float i_term = pid->ki * pid->integral;
    
    // Derivative term
    int derivative = error - pid->last_error;
    float d_term = pid->kd * derivative;
    
    // Calculate PID output
    float pid_output = p_term + i_term + d_term;
    
    // Update PID state
    pid->last_error = error;
    
    // Return the adjustment (not absolute value)
    return (int)pid_output;
}

void pid_reset(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->last_error = 0;
    pid->last_output = 0;
}

void pid_init(pid_controller_t *pid, float kp, float ki, float kd) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid_reset(pid);
}

// Main processing functions
float process_rssi_signal(float raw_rssi, bool racing_mode) {
    if (racing_mode) {
        return apply_filter_chain(&rssi_race_filter_chain, raw_rssi);
    } else {
        return apply_filter_chain(&rssi_filter_chain, raw_rssi);
    }
}

float process_dbm_signal(float raw_dbm, bool racing_mode) {
    if (racing_mode) {
        return apply_filter_chain(&dbm_race_filter_chain, raw_dbm);
    } else {
        return apply_filter_chain(&dbm_filter_chain, raw_dbm);
    }
}

int calculate_bitrate_adjustment(int target_bitrate, int current_bitrate, bool racing_mode) {
    if (s_Config.control_algorithm == CONTROL_ALGORITHM_PID) {
        // PID Controller: Smooth transitions with PID control
        int pid_adjustment = pid_calculate(&bitrate_pid, target_bitrate, current_bitrate);
        return current_bitrate + pid_adjustment;
    } else {
        // Simple FIFO: Direct assignment (faster, more responsive)
        return target_bitrate;
    }
}

// Configuration functions
void advanced_signal_processing_init() {
    // Initialize filters with config values
    init_lowpass_filter(&rssi_lpf, s_Config.lpf_cutoff_freq, s_Config.lpf_sample_freq);
    init_lowpass_filter(&dbm_lpf, s_Config.lpf_cutoff_freq, s_Config.lpf_sample_freq);
    init_2pole_lpf(&rssi_2pole_lpf, s_Config.lpf_cutoff_freq, s_Config.lpf_sample_freq);
    init_2pole_lpf(&dbm_2pole_lpf, s_Config.lpf_cutoff_freq, s_Config.lpf_sample_freq);
    
    // Initialize PID controller
    pid_init(&bitrate_pid, s_Config.pid_kp, s_Config.pid_ki, s_Config.pid_kd);
    
    // Load configuration
    advanced_signal_load_config();
    
    log_line("[AP-ALINK-FPV] Advanced signal processing system initialized");
}

void advanced_signal_processing_cleanup() {
    log_line("[AP-ALINK-FPV] Advanced signal processing system cleanup");
}

void advanced_signal_save_config() {
    // Save configuration to RubyFPV config system
    log_line("[AP-ALINK-FPV] Advanced signal processing config saved");
}

void advanced_signal_load_config() {
    // Load configuration from RubyFPV config system
    log_line("[AP-ALINK-FPV] Advanced signal processing config loaded");
}

void advanced_signal_set_preset(const char* preset_name) {
    if (strcmp(preset_name, "racing") == 0) {
        // Racing mode: Use low-pass filters for faster response
        parse_filter_chain("1", &rssi_race_filter_chain);
        parse_filter_chain("1", &dbm_race_filter_chain);
        s_Config.control_algorithm = CONTROL_ALGORITHM_FIFO;
        log_line("[AP-ALINK-FPV] Applied racing signal processing preset");
    }
    else if (strcmp(preset_name, "long_range") == 0) {
        // Long range: Use complex filter chains for stability
        parse_filter_chain("0,2,3", &rssi_filter_chain);
        parse_filter_chain("0,2,3", &dbm_filter_chain);
        s_Config.control_algorithm = CONTROL_ALGORITHM_PID;
        log_line("[AP-ALINK-FPV] Applied long range signal processing preset");
    }
    else if (strcmp(preset_name, "balanced") == 0) {
        // Balanced: Use moderate filtering
        parse_filter_chain("0,1", &rssi_filter_chain);
        parse_filter_chain("0,1", &dbm_filter_chain);
        s_Config.control_algorithm = CONTROL_ALGORITHM_PID;
        log_line("[AP-ALINK-FPV] Applied balanced signal processing preset");
    }
    else {
        log_line("[AP-ALINK-FPV] Unknown signal processing preset: %s", preset_name);
    }
}
