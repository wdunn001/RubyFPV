#pragma once

#include "../../base/base.h"

// API Response Structure
typedef struct {
    int status_code;        // HTTP-style status code (200, 400, 500, etc.)
    char message[256];      // Human-readable message
    char data[1024];        // JSON or other data payload
} rubalink_api_response_t;

// Function declarations
void rubalink_api_init();
void rubalink_api_cleanup();

// Status API
rubalink_api_response_t rubalink_api_get_status();
rubalink_api_response_t rubalink_api_get_system_info();
rubalink_api_response_t rubalink_api_get_performance_metrics();

// Configuration API
rubalink_api_response_t rubalink_api_get_config();
rubalink_api_response_t rubalink_api_set_config(const char* config_json);
rubalink_api_response_t rubalink_api_reset_config();

// Control API
rubalink_api_response_t rubalink_api_enable_rubalink_advanced(bool enabled);
rubalink_api_response_t rubalink_api_enable_rubyfpv_adaptive(bool enabled);
rubalink_api_response_t rubalink_api_enable_auto_detection(bool enabled);
rubalink_api_response_t rubalink_api_switch_to_advanced();
rubalink_api_response_t rubalink_api_switch_to_rubyfpv();
rubalink_api_response_t rubalink_api_switch_to_auto();

// Preset API
rubalink_api_response_t rubalink_api_get_presets();
rubalink_api_response_t rubalink_api_apply_preset(const char* preset_name);
rubalink_api_response_t rubalink_api_create_preset(const char* preset_name, const char* config_json);
rubalink_api_response_t rubalink_api_delete_preset(const char* preset_name);

// Monitoring API
rubalink_api_response_t rubalink_api_get_monitoring_data();
rubalink_api_response_t rubalink_api_get_signal_processing_status();
rubalink_api_response_t rubalink_api_get_cooldown_status();
rubalink_api_response_t rubalink_api_get_racing_mode_status();

// Profile API
rubalink_api_response_t rubalink_api_get_vehicle_profiles();
rubalink_api_response_t rubalink_api_set_profile_adaptive_type(const char* profile_name, const char* adaptive_type);
rubalink_api_response_t rubalink_api_detect_vehicle_profile(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate);

// Statistics API
rubalink_api_response_t rubalink_api_get_statistics();
rubalink_api_response_t rubalink_api_reset_statistics();
rubalink_api_response_t rubalink_api_get_logs(int max_lines);

// Utility functions
void rubalink_api_set_response(rubalink_api_response_t* response, int status_code, const char* message, const char* data);
const char* rubalink_api_get_status_text(int status_code);
bool rubalink_api_validate_json(const char* json_string);
rubalink_api_response_t rubalink_api_handle_config_request(const char* config_json, char* response_buffer, size_t response_size);
