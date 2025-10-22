#include "signal_pattern_analysis.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Global pattern analysis state
static signal_pattern_buffer_t s_SignalBuffer = {0};
static video_pattern_buffer_t s_VideoBuffer = {0};
static pattern_analysis_config_t s_Config = {
    .enable_signal_analysis = true,
    .enable_video_analysis = true,
    .analysis_window_ms = 10000,  // 10 seconds
    .stability_threshold = 0.7f,
    .trend_threshold = 0.3f,
    .min_samples = 20
};

void signal_pattern_analysis_init() {
    memset(&s_SignalBuffer, 0, sizeof(s_SignalBuffer));
    s_SignalBuffer.initialized = true;
    
    pattern_analysis_config_load();
    
    log_line("[SignalPattern] Signal pattern analysis initialized");
}

void signal_pattern_analysis_cleanup() {
    memset(&s_SignalBuffer, 0, sizeof(s_SignalBuffer));
    log_line("[SignalPattern] Signal pattern analysis cleaned up");
}

void signal_pattern_analysis_add_sample(float rssi, float dbm, uint32_t timestamp) {
    if (!s_SignalBuffer.initialized) {
        return;
    }
    
    // Add new sample
    s_SignalBuffer.rssi_samples[s_SignalBuffer.sample_index] = rssi;
    s_SignalBuffer.dbm_samples[s_SignalBuffer.sample_index] = dbm;
    s_SignalBuffer.timestamps[s_SignalBuffer.sample_index] = timestamp;
    
    // Update indices
    s_SignalBuffer.sample_index = (s_SignalBuffer.sample_index + 1) % 100;
    if (s_SignalBuffer.sample_count < 100) {
        s_SignalBuffer.sample_count++;
    }
    
    // Remove old samples outside analysis window
    uint32_t current_time = timestamp;
    uint8_t valid_samples = 0;
    
    for (uint8_t i = 0; i < s_SignalBuffer.sample_count; i++) {
        uint8_t idx = (s_SignalBuffer.sample_index - s_SignalBuffer.sample_count + i + 100) % 100;
        if (current_time - s_SignalBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            valid_samples++;
        }
    }
    
    s_SignalBuffer.sample_count = valid_samples;
}

signal_pattern_analysis_t signal_pattern_analysis_get_current() {
    signal_pattern_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    if (s_SignalBuffer.sample_count < s_Config.min_samples) {
        return analysis;
    }
    
    // Calculate statistics for valid samples
    uint8_t valid_count = 0;
    float rssi_sum = 0.0f, dbm_sum = 0.0f;
    
    for (uint8_t i = 0; i < s_SignalBuffer.sample_count; i++) {
        uint8_t idx = (s_SignalBuffer.sample_index - s_SignalBuffer.sample_count + i + 100) % 100;
        uint32_t current_time = s_SignalBuffer.timestamps[s_SignalBuffer.sample_index - 1];
        
        if (current_time - s_SignalBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            rssi_sum += s_SignalBuffer.rssi_samples[idx];
            dbm_sum += s_SignalBuffer.dbm_samples[idx];
            valid_count++;
        }
    }
    
    if (valid_count < s_Config.min_samples) {
        return analysis;
    }
    
    // Calculate means
    analysis.rssi_mean = rssi_sum / valid_count;
    analysis.dbm_mean = dbm_sum / valid_count;
    
    // Calculate standard deviations
    float rssi_variance = 0.0f, dbm_variance = 0.0f;
    
    for (uint8_t i = 0; i < s_SignalBuffer.sample_count; i++) {
        uint8_t idx = (s_SignalBuffer.sample_index - s_SignalBuffer.sample_count + i + 100) % 100;
        uint32_t current_time = s_SignalBuffer.timestamps[s_SignalBuffer.sample_index - 1];
        
        if (current_time - s_SignalBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            float rssi_diff = s_SignalBuffer.rssi_samples[idx] - analysis.rssi_mean;
            float dbm_diff = s_SignalBuffer.dbm_samples[idx] - analysis.dbm_mean;
            
            rssi_variance += rssi_diff * rssi_diff;
            dbm_variance += dbm_diff * dbm_diff;
        }
    }
    
    analysis.rssi_std_dev = sqrtf(rssi_variance / valid_count);
    analysis.dbm_std_dev = sqrtf(dbm_variance / valid_count);
    
    // Calculate trends (simplified linear regression)
    analysis.rssi_trend = calculate_trend(s_SignalBuffer.rssi_samples, s_SignalBuffer.timestamps, valid_count);
    analysis.dbm_trend = calculate_trend(s_SignalBuffer.dbm_samples, s_SignalBuffer.timestamps, valid_count);
    
    // Calculate stability scores
    analysis.signal_stability = calculate_stability_score(
        (analysis.rssi_std_dev + analysis.dbm_std_dev) / 2.0f,
        fabsf(analysis.rssi_trend) + fabsf(analysis.dbm_trend)
    );
    
    analysis.has_pattern = true;
    
    return analysis;
}

vehicle_pattern_detection_t signal_pattern_analyze_vehicle_type() {
    vehicle_pattern_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    
    signal_pattern_analysis_t analysis = signal_pattern_analysis_get_current();
    if (!analysis.has_pattern) {
        detection.pattern_type = VEHICLE_PATTERN_UNKNOWN;
        strcpy(detection.description, "Insufficient data");
        return detection;
    }
    
    // Analyze patterns based on signal characteristics
    float rssi_variability = analysis.rssi_std_dev;
    float dbm_variability = analysis.dbm_std_dev;
    float overall_stability = analysis.signal_stability;
    float rssi_trend_magnitude = fabsf(analysis.rssi_trend);
    float dbm_trend_magnitude = fabsf(analysis.dbm_trend);
    
    // Pattern detection logic
    if (rssi_variability > 15.0f && dbm_variability > 8.0f && overall_stability < 0.3f) {
        // High variability, low stability = Racing
        detection.pattern_type = VEHICLE_PATTERN_RACING;
        detection.confidence = 0.8f;
        strcpy(detection.description, "Racing pattern detected");
    }
    else if (rssi_variability < 5.0f && dbm_variability < 3.0f && overall_stability > 0.8f) {
        // Low variability, high stability = Long Range or Surveillance
        if (rssi_trend_magnitude < 0.5f && dbm_trend_magnitude < 0.3f) {
            detection.pattern_type = VEHICLE_PATTERN_SURVEILLANCE;
            detection.confidence = 0.9f;
            strcpy(detection.description, "Surveillance pattern detected");
        } else {
            detection.pattern_type = VEHICLE_PATTERN_LONG_RANGE;
            detection.confidence = 0.7f;
            strcpy(detection.description, "Long range pattern detected");
        }
    }
    else if (rssi_variability < 10.0f && dbm_variability < 5.0f && overall_stability > 0.6f) {
        // Moderate variability, good stability = Cinematic
        detection.pattern_type = VEHICLE_PATTERN_CINEMATIC;
        detection.confidence = 0.7f;
        strcpy(detection.description, "Cinematic pattern detected");
    }
    else if (rssi_variability < 12.0f && dbm_variability < 6.0f && overall_stability > 0.4f) {
        // Moderate characteristics = Freestyle
        detection.pattern_type = VEHICLE_PATTERN_FREESTYLE;
        detection.confidence = 0.6f;
        strcpy(detection.description, "Freestyle pattern detected");
    }
    else {
        // Default to unknown
        detection.pattern_type = VEHICLE_PATTERN_UNKNOWN;
        detection.confidence = 0.3f;
        strcpy(detection.description, "Unknown pattern");
    }
    
    return detection;
}

bool signal_pattern_has_sufficient_data() {
    return s_SignalBuffer.sample_count >= s_Config.min_samples;
}

void video_pattern_analysis_init() {
    memset(&s_VideoBuffer, 0, sizeof(s_VideoBuffer));
    s_VideoBuffer.initialized = true;
    
    log_line("[VideoPattern] Video pattern analysis initialized");
}

void video_pattern_analysis_cleanup() {
    memset(&s_VideoBuffer, 0, sizeof(s_VideoBuffer));
    log_line("[VideoPattern] Video pattern analysis cleaned up");
}

void video_pattern_analysis_add_sample(uint32_t bitrate, uint32_t fps, uint32_t timestamp) {
    if (!s_VideoBuffer.initialized) {
        return;
    }
    
    // Add new sample
    s_VideoBuffer.bitrate_samples[s_VideoBuffer.sample_index] = bitrate;
    s_VideoBuffer.fps_samples[s_VideoBuffer.sample_index] = fps;
    s_VideoBuffer.timestamps[s_VideoBuffer.sample_index] = timestamp;
    
    // Update indices
    s_VideoBuffer.sample_index = (s_VideoBuffer.sample_index + 1) % 50;
    if (s_VideoBuffer.sample_count < 50) {
        s_VideoBuffer.sample_count++;
    }
    
    // Remove old samples outside analysis window
    uint32_t current_time = timestamp;
    uint8_t valid_samples = 0;
    
    for (uint8_t i = 0; i < s_VideoBuffer.sample_count; i++) {
        uint8_t idx = (s_VideoBuffer.sample_index - s_VideoBuffer.sample_count + i + 50) % 50;
        if (current_time - s_VideoBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            valid_samples++;
        }
    }
    
    s_VideoBuffer.sample_count = valid_samples;
}

video_pattern_analysis_t video_pattern_analysis_get_current() {
    video_pattern_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    if (s_VideoBuffer.sample_count < s_Config.min_samples) {
        return analysis;
    }
    
    // Calculate statistics for valid samples
    uint8_t valid_count = 0;
    float bitrate_sum = 0.0f, fps_sum = 0.0f;
    
    for (uint8_t i = 0; i < s_VideoBuffer.sample_count; i++) {
        uint8_t idx = (s_VideoBuffer.sample_index - s_VideoBuffer.sample_count + i + 50) % 50;
        uint32_t current_time = s_VideoBuffer.timestamps[s_VideoBuffer.sample_index - 1];
        
        if (current_time - s_VideoBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            bitrate_sum += s_VideoBuffer.bitrate_samples[idx];
            fps_sum += s_VideoBuffer.fps_samples[idx];
            valid_count++;
        }
    }
    
    if (valid_count < s_Config.min_samples) {
        return analysis;
    }
    
    // Calculate means
    analysis.bitrate_mean = bitrate_sum / valid_count;
    analysis.fps_mean = fps_sum / valid_count;
    
    // Calculate standard deviations
    float bitrate_variance = 0.0f, fps_variance = 0.0f;
    
    for (uint8_t i = 0; i < s_VideoBuffer.sample_count; i++) {
        uint8_t idx = (s_VideoBuffer.sample_index - s_VideoBuffer.sample_count + i + 50) % 50;
        uint32_t current_time = s_VideoBuffer.timestamps[s_VideoBuffer.sample_index - 1];
        
        if (current_time - s_VideoBuffer.timestamps[idx] <= s_Config.analysis_window_ms) {
            float bitrate_diff = s_VideoBuffer.bitrate_samples[idx] - analysis.bitrate_mean;
            float fps_diff = s_VideoBuffer.fps_samples[idx] - analysis.fps_mean;
            
            bitrate_variance += bitrate_diff * bitrate_diff;
            fps_variance += fps_diff * fps_diff;
        }
    }
    
    analysis.bitrate_std_dev = sqrtf(bitrate_variance / valid_count);
    analysis.fps_std_dev = sqrtf(fps_variance / valid_count);
    
    // Calculate trends
    analysis.bitrate_trend = calculate_trend((float*)s_VideoBuffer.bitrate_samples, s_VideoBuffer.timestamps, valid_count);
    analysis.fps_trend = calculate_trend((float*)s_VideoBuffer.fps_samples, s_VideoBuffer.timestamps, valid_count);
    
    // Calculate stability score
    analysis.video_stability = calculate_stability_score(
        (analysis.bitrate_std_dev + analysis.fps_std_dev) / 2.0f,
        fabsf(analysis.bitrate_trend) + fabsf(analysis.fps_trend)
    );
    
    analysis.has_pattern = true;
    
    return analysis;
}

bool video_pattern_has_sufficient_data() {
    return s_VideoBuffer.sample_count >= s_Config.min_samples;
}

void pattern_analysis_config_load() {
    // Load configuration from RubALink config system
    rubalink_config_get_bool("enable_signal_analysis", &s_Config.enable_signal_analysis);
    rubalink_config_get_bool("enable_video_analysis", &s_Config.enable_video_analysis);
    rubalink_config_get_int("analysis_window_ms", (int*)&s_Config.analysis_window_ms);
    rubalink_config_get_float("stability_threshold", &s_Config.stability_threshold);
    rubalink_config_get_float("trend_threshold", &s_Config.trend_threshold);
    rubalink_config_get_int("min_samples", (int*)&s_Config.min_samples);
}

void pattern_analysis_config_save() {
    // Save configuration to RubALink config system
    rubalink_config_set_bool("enable_signal_analysis", s_Config.enable_signal_analysis);
    rubalink_config_set_bool("enable_video_analysis", s_Config.enable_video_analysis);
    rubalink_config_set_int("analysis_window_ms", s_Config.analysis_window_ms);
    rubalink_config_set_float("stability_threshold", s_Config.stability_threshold);
    rubalink_config_set_float("trend_threshold", s_Config.trend_threshold);
    rubalink_config_set_int("min_samples", s_Config.min_samples);
}

pattern_analysis_config_t pattern_analysis_get_config() {
    return s_Config;
}

void pattern_analysis_set_config(const pattern_analysis_config_t* config) {
    s_Config = *config;
    pattern_analysis_config_save();
}

// Utility functions
float calculate_mean(const float* samples, uint8_t count) {
    if (count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    return sum / count;
}

float calculate_std_dev(const float* samples, uint8_t count, float mean) {
    if (count <= 1) return 0.0f;
    
    float variance = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float diff = samples[i] - mean;
        variance += diff * diff;
    }
    
    return sqrtf(variance / count);
}

float calculate_trend(const float* samples, const uint32_t* timestamps, uint8_t count) {
    if (count < 2) return 0.0f;
    
    // Simple linear regression slope calculation
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    
    for (uint8_t i = 0; i < count; i++) {
        float x = (float)timestamps[i];
        float y = samples[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    float n = (float)count;
    float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    return slope;
}

float calculate_stability_score(float std_dev, float trend) {
    // Normalize standard deviation (assuming typical range 0-20)
    float normalized_std_dev = fminf(std_dev / 20.0f, 1.0f);
    
    // Normalize trend magnitude (assuming typical range 0-5)
    float normalized_trend = fminf(trend / 5.0f, 1.0f);
    
    // Stability is inverse of variability and trend
    float stability = 1.0f - (normalized_std_dev + normalized_trend) / 2.0f;
    
    return fmaxf(0.0f, fminf(1.0f, stability));
}

const char* vehicle_pattern_type_to_string(vehicle_pattern_type_t type) {
    switch (type) {
        case VEHICLE_PATTERN_RACING: return "Racing";
        case VEHICLE_PATTERN_CINEMATIC: return "Cinematic";
        case VEHICLE_PATTERN_LONG_RANGE: return "Long Range";
        case VEHICLE_PATTERN_FREESTYLE: return "Freestyle";
        case VEHICLE_PATTERN_SURVEILLANCE: return "Surveillance";
        default: return "Unknown";
    }
}
