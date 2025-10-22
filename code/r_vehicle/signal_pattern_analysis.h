#pragma once

#include "../base/base.h"
#include "../base/rubalink_config.h"
#include <stdint.h>
#include <stdbool.h>

// Signal pattern analysis structures
typedef struct {
    float rssi_samples[100];      // Last 100 RSSI samples
    float dbm_samples[100];       // Last 100 dBm samples
    uint32_t timestamps[100];     // Timestamps for samples
    uint8_t sample_count;         // Current number of samples
    uint8_t sample_index;         // Current sample index
    bool initialized;             // Initialization flag
} signal_pattern_buffer_t;

typedef struct {
    float rssi_mean;              // Mean RSSI value
    float rssi_std_dev;           // RSSI standard deviation
    float dbm_mean;               // Mean dBm value
    float dbm_std_dev;            // dBm standard deviation
    float rssi_trend;             // RSSI trend (slope)
    float dbm_trend;              // dBm trend (slope)
    float signal_stability;       // Signal stability score (0-1)
    bool has_pattern;             // Whether a pattern was detected
} signal_pattern_analysis_t;

// Vehicle detection patterns
typedef enum {
    VEHICLE_PATTERN_UNKNOWN = 0,
    VEHICLE_PATTERN_RACING,       // High variability, quick changes
    VEHICLE_PATTERN_CINEMATIC,    // Smooth, gradual changes
    VEHICLE_PATTERN_LONG_RANGE,   // Stable, low variability
    VEHICLE_PATTERN_FREESTYLE,    // Moderate variability
    VEHICLE_PATTERN_SURVEILLANCE  // Very stable, minimal changes
} vehicle_pattern_type_t;

typedef struct {
    vehicle_pattern_type_t pattern_type;
    float confidence;             // Confidence score (0-1)
    char description[64];         // Human-readable description
} vehicle_pattern_detection_t;

// Video pattern analysis structures
typedef struct {
    uint32_t bitrate_samples[50]; // Last 50 bitrate samples
    uint32_t fps_samples[50];     // Last 50 FPS samples
    uint32_t timestamps[50];      // Timestamps for samples
    uint8_t sample_count;         // Current number of samples
    uint8_t sample_index;         // Current sample index
    bool initialized;             // Initialization flag
} video_pattern_buffer_t;

typedef struct {
    float bitrate_mean;           // Mean bitrate
    float bitrate_std_dev;        // Bitrate standard deviation
    float fps_mean;               // Mean FPS
    float fps_std_dev;            // FPS standard deviation
    float bitrate_trend;          // Bitrate trend
    float fps_trend;              // FPS trend
    float video_stability;        // Video stability score (0-1)
    bool has_pattern;             // Whether a pattern was detected
} video_pattern_analysis_t;

// Pattern analysis configuration
typedef struct {
    bool enable_signal_analysis;  // Enable signal pattern analysis
    bool enable_video_analysis;   // Enable video pattern analysis
    uint32_t analysis_window_ms;  // Analysis window in milliseconds
    float stability_threshold;     // Stability threshold for pattern detection
    float trend_threshold;        // Trend threshold for pattern detection
    uint8_t min_samples;          // Minimum samples for analysis
} pattern_analysis_config_t;

// Function declarations
void signal_pattern_analysis_init();
void signal_pattern_analysis_cleanup();
void signal_pattern_analysis_add_sample(float rssi, float dbm, uint32_t timestamp);
signal_pattern_analysis_t signal_pattern_analysis_get_current();
vehicle_pattern_detection_t signal_pattern_analyze_vehicle_type();
bool signal_pattern_has_sufficient_data();

void video_pattern_analysis_init();
void video_pattern_analysis_cleanup();
void video_pattern_analysis_add_sample(uint32_t bitrate, uint32_t fps, uint32_t timestamp);
video_pattern_analysis_t video_pattern_analysis_get_current();
bool video_pattern_has_sufficient_data();

void pattern_analysis_config_load();
void pattern_analysis_config_save();
pattern_analysis_config_t pattern_analysis_get_config();
void pattern_analysis_set_config(const pattern_analysis_config_t* config);

// Utility functions
float calculate_mean(const float* samples, uint8_t count);
float calculate_std_dev(const float* samples, uint8_t count, float mean);
float calculate_trend(const float* samples, const uint32_t* timestamps, uint8_t count);
float calculate_stability_score(float std_dev, float trend);
const char* vehicle_pattern_type_to_string(vehicle_pattern_type_t type);
