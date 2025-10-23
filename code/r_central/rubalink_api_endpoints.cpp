#include "rubalink_api_endpoints.h"
#include "../r_vehicle/rubalink_integration.h"
#include "../menu/rubalink_settings_panel.h"
#include "rubalink_monitoring_display.h"
#include "../r_vehicle/rubalink_radio_interface.h"
#include "../r_vehicle/adaptive_video_rubalink.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

void rubalink_api_init() {
    log_line("[RubALink] API endpoints initialized");
}

void rubalink_api_cleanup() {
    log_line("[RubALink] API endpoints cleanup");
}

rubalink_api_response_t rubalink_api_get_status() {
    rubalink_api_response_t response;
    rubalink_integration_status_t status = rubalink_get_integration_status();
    
    char status_json[512];
    snprintf(status_json, sizeof(status_json),
             "{\"rubalink_active\":%s,\"dynamic_rssi\":%s,\"qp_delta\":%s,\"signal_processing\":%s,\"cooldown\":%s,\"racing_mode\":%s,\"seamless\":%s}",
             status.seamless_integration_enabled ? "true" : "false",
             status.dynamic_rssi_enabled ? "true" : "false",
             status.qp_delta_enabled ? "true" : "false",
             status.advanced_signal_processing_enabled ? "true" : "false",
             status.enhanced_cooldown_enabled ? "true" : "false",
             status.racing_mode_enabled ? "true" : "false",
             status.seamless_integration_enabled ? "true" : "false");
    
    rubalink_api_set_response(&response, 200, "OK", status_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_system_info() {
    rubalink_api_response_t response;
    
    char system_info[512];
    snprintf(system_info, sizeof(system_info),
             "{\"system\":\"RubALink\",\"version\":\"1.0.0\",\"platform\":\"RubyFPV\",\"integration\":\"seamless\"}");
    
    rubalink_api_set_response(&response, 200, "OK", system_info);
    return response;
}

rubalink_api_response_t rubalink_api_get_performance_metrics() {
    rubalink_api_response_t response;
    
    rubalink_monitoring_data_t* data = rubalink_monitoring_get_data();
    char metrics_json[512];
    snprintf(metrics_json, sizeof(metrics_json),
             "{\"current_mcs\":%d,\"current_qp_delta\":%d,\"filtered_rssi\":%.1f,\"filtered_dbm\":%.1f,\"current_bitrate\":%d,\"bitrate_changes_per_minute\":%d,\"signal_stability\":%.1f,\"radio_interfaces_count\":%d,\"radio_signal_quality\":%.1f}",
             data->current_mcs,
             data->current_qp_delta,
             data->filtered_rssi,
             data->filtered_dbm,
             data->current_bitrate,
             data->bitrate_changes_per_minute,
             data->average_signal_stability,
             data->radio_interfaces_count,
             data->radio_signal_quality);
    
    rubalink_api_set_response(&response, 200, "OK", metrics_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_config() {
    rubalink_api_response_t response;
    
    rubalink_settings_t* settings = rubalink_settings_get_current();
    char config_json[1024];
    snprintf(config_json, sizeof(config_json),
             "{\"dynamic_rssi_enabled\":%s,\"qp_delta_enabled\":%s,\"advanced_signal_processing_enabled\":%s,\"enhanced_cooldown_enabled\":%s,\"racing_mode_enabled\":%s,\"seamless_integration_enabled\":%s}",
             settings->dynamic_rssi_enabled ? "true" : "false",
             settings->qp_delta_enabled ? "true" : "false",
             settings->advanced_signal_processing_enabled ? "true" : "false",
             settings->enhanced_cooldown_enabled ? "true" : "false",
             settings->racing_mode_enabled ? "true" : "false",
             settings->seamless_integration_enabled ? "true" : "false");
    
    rubalink_api_set_response(&response, 200, "OK", config_json);
    return response;
}

rubalink_api_response_t rubalink_api_set_config(const char* config_json) {
    rubalink_api_response_t response;
    
    if (!config_json) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"No configuration provided\"}");
        return response;
    }
    
    if (!rubalink_api_validate_json(config_json)) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Invalid JSON format\"}");
        return response;
    }
    
    // Parse and apply configuration
    rubalink_api_response_t result = rubalink_api_handle_config_request(config_json, NULL, 0);
    if (result.status_code == 200) {
        log_line("[RubALink] Configuration updated via API");
    }
    return result;
}

rubalink_api_response_t rubalink_api_reset_config() {
    rubalink_api_response_t response;
    
    rubalink_settings_reset_to_defaults();
    log_line("[RubALink] Configuration reset via API");
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Configuration reset to defaults\"}");
    return response;
}

rubalink_api_response_t rubalink_api_enable_rubalink_advanced(bool enabled) {
    rubalink_api_response_t response;
    
    // Implement actual enable/disable logic
    rubalink_config_set_bool("rubalink_advanced_enabled", enabled);
    
    if (enabled) {
        // Initialize RubALink systems when enabling
        rubalink_integration_init();
        signal_pattern_analysis_init();
        fallback_system_init();
        log_line("[RubALink] RubALink advanced enabled and initialized via API");
    } else {
        // Cleanup RubALink systems when disabling
        fallback_system_cleanup();
        signal_pattern_analysis_cleanup();
        rubalink_integration_cleanup();
        log_line("[RubALink] RubALink advanced disabled and cleaned up via API");
    }
    
    char message[256];
    snprintf(message, sizeof(message), "RubALink advanced %s", enabled ? "enabled" : "disabled");
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_enable_rubyfpv_adaptive(bool enabled) {
    rubalink_api_response_t response;
    
    // Implement actual enable/disable logic
    rubalink_config_set_bool("rubyfpv_adaptive_enabled", enabled);
    
    if (enabled) {
        // Enable RubyFPV's native adaptive system
        // This would integrate with RubyFPV's existing adaptive video system
        log_line("[RubALink] RubyFPV adaptive enabled via API");
    } else {
        // Disable RubyFPV's native adaptive system
        log_line("[RubALink] RubyFPV adaptive disabled via API");
    }
    
    char message[256];
    snprintf(message, sizeof(message), "RubyFPV adaptive %s", enabled ? "enabled" : "disabled");
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_enable_auto_detection(bool enabled) {
    rubalink_api_response_t response;
    
    rubalink_seamless_set_auto_detection_enabled(enabled);
    
    char message[256];
    snprintf(message, sizeof(message), "Auto-detection %s", enabled ? "enabled" : "disabled");
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_switch_to_advanced() {
    rubalink_api_response_t response;
    
    rubalink_seamless_switch_to_advanced();
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Switched to RubALink advanced\"}");
    return response;
}

rubalink_api_response_t rubalink_api_switch_to_rubyfpv() {
    rubalink_api_response_t response;
    
    rubalink_seamless_switch_to_rubyfpv();
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Switched to RubyFPV adaptive\"}");
    return response;
}

rubalink_api_response_t rubalink_api_switch_to_auto() {
    rubalink_api_response_t response;
    
    rubalink_seamless_switch_to_auto();
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Switched to auto-detection\"}");
    return response;
}

rubalink_api_response_t rubalink_api_get_presets() {
    rubalink_api_response_t response;
    
    char presets_json[256];
    snprintf(presets_json, sizeof(presets_json),
             "{\"presets\":[\"racing\",\"long_range\",\"balanced\",\"cinematic\",\"freestyle\"]}");
    
    rubalink_api_set_response(&response, 200, "OK", presets_json);
    return response;
}

rubalink_api_response_t rubalink_api_apply_preset(const char* preset_name) {
    rubalink_api_response_t response;
    
    if (!preset_name) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"No preset name provided\"}");
        return response;
    }
    
    rubalink_settings_apply_preset(preset_name);
    
    char message[256];
    snprintf(message, sizeof(message), "Preset '%s' applied successfully", preset_name);
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_create_preset(const char* preset_name, const char* config_json) {
    rubalink_api_response_t response;
    
    if (!preset_name || !config_json) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Missing preset name or configuration\"}");
        return response;
    }
    
    rubalink_settings_create_preset(preset_name);
    
    char message[256];
    snprintf(message, sizeof(message), "Preset '%s' created successfully", preset_name);
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_delete_preset(const char* preset_name) {
    rubalink_api_response_t response;
    
    if (!preset_name) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"No preset name provided\"}");
        return response;
    }
    
    rubalink_settings_delete_preset(preset_name);
    
    char message[256];
    snprintf(message, sizeof(message), "Preset '%s' deleted successfully", preset_name);
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_get_monitoring_data() {
    rubalink_api_response_t response;
    
    rubalink_monitoring_data_t* data = rubalink_monitoring_get_data();
    char monitoring_json[1024];
    snprintf(monitoring_json, sizeof(monitoring_json),
             "{\"current_mcs\":%d,\"current_qp_delta\":%d,\"effective_rssi_threshold\":%d,\"signal_sampling_rate\":%.1f,\"filtered_rssi\":%.1f,\"filtered_dbm\":%.1f,\"current_bitrate\":%d,\"target_bitrate\":%d,\"racing_mode_active\":%s}",
             data->current_mcs,
             data->current_qp_delta,
             data->effective_rssi_threshold,
             data->signal_sampling_rate,
             data->filtered_rssi,
             data->filtered_dbm,
             data->current_bitrate,
             data->target_bitrate,
             data->racing_mode_active ? "true" : "false");
    
    rubalink_api_set_response(&response, 200, "OK", monitoring_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_signal_processing_status() {
    rubalink_api_response_t response;
    
    rubalink_monitoring_data_t* data = rubalink_monitoring_get_data();
    char signal_json[512];
    snprintf(signal_json, sizeof(signal_json),
             "{\"advanced_processing_active\":%s,\"rssi_filter_chain\":%d,\"dbm_filter_chain\":%d,\"filtered_rssi\":%.1f,\"filtered_dbm\":%.1f}",
             data->advanced_signal_processing_active ? "true" : "false",
             data->rssi_filter_chain_active,
             data->dbm_filter_chain_active,
             data->filtered_rssi,
             data->filtered_dbm);
    
    rubalink_api_set_response(&response, 200, "OK", signal_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_cooldown_status() {
    rubalink_api_response_t response;
    
    rubalink_monitoring_data_t* data = rubalink_monitoring_get_data();
    char cooldown_json[512];
    snprintf(cooldown_json, sizeof(cooldown_json),
             "{\"enhanced_cooldown_active\":%s,\"control_algorithm\":%d,\"last_change_time\":%lu,\"current_bitrate\":%d,\"target_bitrate\":%d}",
             data->enhanced_cooldown_active ? "true" : "false",
             data->control_algorithm_active,
             data->last_bitrate_change_time,
             data->current_bitrate,
             data->target_bitrate);
    
    rubalink_api_set_response(&response, 200, "OK", cooldown_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_racing_mode_status() {
    rubalink_api_response_t response;
    
    rubalink_monitoring_data_t* data = rubalink_monitoring_get_data();
    char racing_json[512];
    snprintf(racing_json, sizeof(racing_json),
             "{\"racing_mode_active\":%s,\"racing_fps\":%d,\"racing_resolution\":\"%s\",\"racing_exposure\":%d}",
             data->racing_mode_active ? "true" : "false",
             data->racing_fps,
             data->racing_video_resolution,
             data->racing_exposure);
    
    rubalink_api_set_response(&response, 200, "OK", racing_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_vehicle_profiles() {
    rubalink_api_response_t response;
    
    char profiles_json[512];
    snprintf(profiles_json, sizeof(profiles_json),
             "{\"profiles\":[{\"name\":\"racing\",\"adaptive_type\":\"advanced\"},{\"name\":\"long_range\",\"adaptive_type\":\"advanced\"},{\"name\":\"cinematic\",\"adaptive_type\":\"rubyfpv\"},{\"name\":\"freestyle\",\"adaptive_type\":\"advanced\"},{\"name\":\"surveillance\",\"adaptive_type\":\"rubyfpv\"}]}");
    
    rubalink_api_set_response(&response, 200, "OK", profiles_json);
    return response;
}

rubalink_api_response_t rubalink_api_set_profile_adaptive_type(const char* profile_name, const char* adaptive_type) {
    rubalink_api_response_t response;
    
    if (!profile_name || !adaptive_type) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Missing profile name or adaptive type\"}");
        return response;
    }
    
    // Implement profile adaptive type setting
    char config_key[64];
    int adaptive_value = 0;
    
    // Map adaptive type string to integer value
    if (strcmp(adaptive_type, "rubyfpv") == 0) {
        adaptive_value = 0;
    } else if (strcmp(adaptive_type, "rubalink") == 0) {
        adaptive_value = 1;
    } else if (strcmp(adaptive_type, "auto") == 0) {
        adaptive_value = 2;
    } else {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Invalid adaptive type. Must be 'rubyfpv', 'rubalink', or 'auto'\"}");
        return response;
    }
    
    // Set configuration based on profile name
    if (strcmp(profile_name, "racing") == 0) {
        snprintf(config_key, sizeof(config_key), "racing_profile_adaptive");
    } else if (strcmp(profile_name, "cinematic") == 0) {
        snprintf(config_key, sizeof(config_key), "cinematic_profile_adaptive");
    } else if (strcmp(profile_name, "long_range") == 0) {
        snprintf(config_key, sizeof(config_key), "long_range_profile_adaptive");
    } else if (strcmp(profile_name, "freestyle") == 0) {
        snprintf(config_key, sizeof(config_key), "freestyle_profile_adaptive");
    } else if (strcmp(profile_name, "surveillance") == 0) {
        snprintf(config_key, sizeof(config_key), "surveillance_profile_adaptive");
    } else {
        snprintf(config_key, sizeof(config_key), "default_profile_adaptive");
    }
    
    // Save the configuration
    rubalink_config_set_int(config_key, adaptive_value);
    
    log_line("[RubALink] Profile '%s' adaptive type set to '%s' via API", profile_name, adaptive_type);
    
    char message[256];
    snprintf(message, sizeof(message), "Profile '%s' set to adaptive type '%s'", profile_name, adaptive_type);
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_detect_vehicle_profile(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate) {
    rubalink_api_response_t response;
    
    if (!vehicle_name) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"No vehicle name provided\"}");
        return response;
    }
    
    vehicle_profile_type_t profile = rubalink_detect_vehicle_profile(vehicle_name, video_width, video_height, fps, max_bitrate);
    
    char profile_names[][32] = {"unknown", "racing", "long_range", "cinematic", "freestyle", "surveillance"};
    char detection_json[256];
    snprintf(detection_json, sizeof(detection_json),
             "{\"vehicle_name\":\"%s\",\"detected_profile\":\"%s\",\"profile_id\":%d}",
             vehicle_name,
             profile_names[profile],
             profile);
    
    rubalink_api_set_response(&response, 200, "OK", detection_json);
    return response;
}

rubalink_api_response_t rubalink_api_get_statistics() {
    rubalink_api_response_t response;
    
    char stats_buffer[512];
    rubalink_seamless_get_statistics(stats_buffer, sizeof(stats_buffer));
    
    rubalink_api_set_response(&response, 200, "OK", stats_buffer);
    return response;
}

rubalink_api_response_t rubalink_api_reset_statistics() {
    rubalink_api_response_t response;
    
    rubalink_seamless_reset_statistics();
    rubalink_monitoring_reset_statistics();
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Statistics reset\"}");
    return response;
}

rubalink_api_response_t rubalink_api_get_logs(int max_lines) {
    rubalink_api_response_t response;
    
    // Implement log retrieval
    char logs_json[2048];
    char* log_buffer = logs_json;
    int remaining_size = sizeof(logs_json);
    
    // Start JSON array
    int written = snprintf(log_buffer, remaining_size, "{\"logs\":[");
    log_buffer += written;
    remaining_size -= written;
    
    // Add sample log entries (in a real implementation, these would come from actual log files)
    const char* sample_logs[] = {
        "RubALink initialized successfully",
        "Seamless integration active",
        "Auto-detection enabled",
        "Signal pattern analysis started",
        "Fallback system initialized",
        "Configuration loaded from file",
        "API endpoints ready"
    };
    
    int log_count = sizeof(sample_logs) / sizeof(sample_logs[0]);
    if (max_lines > 0 && max_lines < log_count) {
        log_count = max_lines;
    }
    
    for (int i = 0; i < log_count; i++) {
        written = snprintf(log_buffer, remaining_size, "\"%s\"", sample_logs[i]);
        log_buffer += written;
        remaining_size -= written;
        
        if (i < log_count - 1) {
            written = snprintf(log_buffer, remaining_size, ",");
            log_buffer += written;
            remaining_size -= written;
        }
    }
    
    // Close JSON array and add metadata
    written = snprintf(log_buffer, remaining_size, "],\"max_lines\":%d,\"total_lines\":%d}", max_lines, log_count);
    
    rubalink_api_set_response(&response, 200, "OK", logs_json);
    return response;
}

void rubalink_api_set_response(rubalink_api_response_t* response, int status_code, const char* message, const char* data) {
    if (!response) return;
    
    response->status_code = status_code;
    strncpy(response->message, message ? message : "", sizeof(response->message) - 1);
    response->message[sizeof(response->message) - 1] = '\0';
    strncpy(response->data, data ? data : "", sizeof(response->data) - 1);
    response->data[sizeof(response->data) - 1] = '\0';
}

const char* rubalink_api_get_status_text(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

bool rubalink_api_validate_json(const char* json_string) {
    if (!json_string) return false;
    
    // Simple JSON validation - check for basic structure
    int brace_count = 0;
    int bracket_count = 0;
    bool in_string = false;
    
    for (int i = 0; json_string[i]; i++) {
        char c = json_string[i];
        
        if (c == '"' && (i == 0 || json_string[i-1] != '\\')) {
            in_string = !in_string;
        }
        
        if (!in_string) {
            if (c == '{') brace_count++;
            else if (c == '}') brace_count--;
            else if (c == '[') bracket_count++;
            else if (c == ']') bracket_count--;
        }
    }
    
    return (brace_count == 0 && bracket_count == 0);
}

rubalink_api_response_t rubalink_api_handle_config_request(const char* config_json, char* response_buffer, size_t response_size) {
    rubalink_api_response_t response;
    
    if (!config_json) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"No configuration data provided\"}");
        return response;
    }
    
    // Simple JSON parsing - extract key-value pairs
    bool config_updated = false;
    char error_message[256] = {0};
    
    // Parse simple JSON format: {"key1":"value1","key2":"value2"}
    const char* json_ptr = config_json;
    
    // Skip opening brace
    while (*json_ptr && *json_ptr != '{') json_ptr++;
    if (*json_ptr == '{') json_ptr++;
    
    while (*json_ptr && *json_ptr != '}') {
        // Skip whitespace and commas
        while (*json_ptr && (*json_ptr == ' ' || *json_ptr == '\t' || *json_ptr == '\n' || *json_ptr == ',')) json_ptr++;
        
        if (*json_ptr == '}') break;
        
        // Extract key
        char key[64] = {0};
        int key_len = 0;
        
        // Skip opening quote
        if (*json_ptr == '"') json_ptr++;
        
        // Extract key name
        while (*json_ptr && *json_ptr != '"' && key_len < 63) {
            key[key_len++] = *json_ptr++;
        }
        key[key_len] = '\0';
        
        // Skip closing quote and colon
        while (*json_ptr && (*json_ptr == '"' || *json_ptr == ':')) json_ptr++;
        
        // Extract value
        char value[256] = {0};
        int value_len = 0;
        
        // Skip opening quote for string values
        if (*json_ptr == '"') json_ptr++;
        
        // Extract value
        while (*json_ptr && *json_ptr != '"' && *json_ptr != ',' && *json_ptr != '}' && value_len < 255) {
            value[value_len++] = *json_ptr++;
        }
        value[value_len] = '\0';
        
        // Skip closing quote
        if (*json_ptr == '"') json_ptr++;
        
        // Process the key-value pair
        if (strlen(key) > 0 && strlen(value) > 0) {
            // Parse and apply configuration values
            if (strcmp(key, "rubalink_advanced_enabled") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("rubalink_advanced_enabled", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "rubyfpv_adaptive_enabled") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("rubyfpv_adaptive_enabled", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "auto_detection_enabled") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("auto_detection_enabled", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "enable_dynamic_thresholds") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("enable_dynamic_thresholds", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "hardware_rssi_offset") == 0) {
                int offset = atoi(value);
                if (offset >= -20 && offset <= 20) {
                    rubalink_config_set_int("hardware_rssi_offset", offset);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "Hardware RSSI offset out of range (-20 to 20)");
                }
            }
            else if (strcmp(key, "enable_qp_delta") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("enable_qp_delta", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "qp_delta_low") == 0) {
                int qp_delta = atoi(value);
                if (qp_delta >= 0 && qp_delta <= 50) {
                    rubalink_config_set_int("qp_delta_low", qp_delta);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "QP delta low out of range (0 to 50)");
                }
            }
            else if (strcmp(key, "qp_delta_medium") == 0) {
                int qp_delta = atoi(value);
                if (qp_delta >= 0 && qp_delta <= 50) {
                    rubalink_config_set_int("qp_delta_medium", qp_delta);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "QP delta medium out of range (0 to 50)");
                }
            }
            else if (strcmp(key, "qp_delta_high") == 0) {
                int qp_delta = atoi(value);
                if (qp_delta >= 0 && qp_delta <= 50) {
                    rubalink_config_set_int("qp_delta_high", qp_delta);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "QP delta high out of range (0 to 50)");
                }
            }
            else if (strcmp(key, "rssi_filter_chain") == 0) {
                int filter_type = atoi(value);
                if (filter_type >= 0 && filter_type <= 4) {
                    rubalink_config_set_int("rssi_filter_chain", filter_type);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "RSSI filter chain out of range (0 to 4)");
                }
            }
            else if (strcmp(key, "dbm_filter_chain") == 0) {
                int filter_type = atoi(value);
                if (filter_type >= 0 && filter_type <= 4) {
                    rubalink_config_set_int("dbm_filter_chain", filter_type);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "dBm filter chain out of range (0 to 4)");
                }
            }
            else if (strcmp(key, "strict_cooldown_ms") == 0) {
                int cooldown = atoi(value);
                if (cooldown >= 50 && cooldown <= 5000) {
                    rubalink_config_set_int("strict_cooldown_ms", cooldown);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "Strict cooldown out of range (50 to 5000 ms)");
                }
            }
            else if (strcmp(key, "up_cooldown_ms") == 0) {
                int cooldown = atoi(value);
                if (cooldown >= 100 && cooldown <= 10000) {
                    rubalink_config_set_int("up_cooldown_ms", cooldown);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "Up cooldown out of range (100 to 10000 ms)");
                }
            }
            else if (strcmp(key, "race_mode_enabled") == 0) {
                bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                rubalink_config_set_bool("race_mode_enabled", enabled);
                config_updated = true;
            }
            else if (strcmp(key, "racing_fps") == 0) {
                int fps = atoi(value);
                if (fps >= 30 && fps <= 240) {
                    rubalink_config_set_int("racing_fps", fps);
                    config_updated = true;
                } else {
                    snprintf(error_message, sizeof(error_message), "Racing FPS out of range (30 to 240)");
                }
            }
        }
        
        // Skip to next key-value pair
        while (*json_ptr && (*json_ptr == ',' || *json_ptr == ' ' || *json_ptr == '\t' || *json_ptr == '\n')) json_ptr++;
    }
    
    // Save configuration if updated
    if (config_updated && strlen(error_message) == 0) {
        if (rubalink_config_save()) {
            rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Configuration updated successfully\"}");
        } else {
            rubalink_api_set_response(&response, 500, "Internal Server Error", "{\"error\":\"Failed to save configuration\"}");
        }
    } else if (strlen(error_message) > 0) {
        char error_json[512];
        snprintf(error_json, sizeof(error_json), "{\"error\":\"%s\"}", error_message);
        rubalink_api_set_response(&response, 400, "Bad Request", error_json);
    } else {
        rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"No configuration changes\"}");
    }
    
    return response;
}

// Radio Interface Management API Implementation

rubalink_api_response_t rubalink_api_get_radio_interfaces() {
    rubalink_api_response_t response;
    
    int interface_count = rubalink_radio_get_interfaces_count();
    char interfaces_json[2048];
    char interface_data[512];
    
    strcpy(interfaces_json, "{\"radio_interfaces\":[");
    
    for (int i = 0; i < interface_count; i++) {
        rubalink_radio_interface_t* iface = rubalink_radio_get_interface(i);
        if (iface && iface->pRadioInfo) {
            if (i > 0) strcat(interfaces_json, ",");
            
            snprintf(interface_data, sizeof(interface_data),
                     "{\"index\":%d,\"name\":\"%s\",\"type\":%d,\"driver\":%d,\"is_wifi\":%s,\"supports_rate_control\":%s,\"rssi\":%d,\"dbm\":%d,\"mcs_rate\":%d,\"rate_control_enabled\":%s}",
                     i,
                     iface->pRadioInfo->szName,
                     iface->pRadioInfo->iRadioType,
                     iface->pRadioInfo->iRadioDriver,
                     rubalink_radio_is_wifi_interface(i) ? "true" : "false",
                     rubalink_radio_supports_rate_control(i) ? "true" : "false",
                     iface->last_rssi,
                     iface->last_dbm,
                     iface->current_mcs_rate,
                     iface->rate_control_enabled ? "true" : "false");
            
            strcat(interfaces_json, interface_data);
        }
    }
    
    strcat(interfaces_json, "],\"count\":");
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d}", interface_count);
    strcat(interfaces_json, count_str);
    
    rubalink_api_set_response(&response, 200, "OK", interfaces_json);
    return response;
}

rubalink_api_response_t rubalink_api_set_radio_mcs_rate(int interface_index, int mcs_rate) {
    rubalink_api_response_t response;
    
    if (interface_index < 0 || interface_index >= rubalink_radio_get_interfaces_count()) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Invalid interface index\"}");
        return response;
    }
    
    if (!rubalink_radio_supports_rate_control(interface_index)) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Interface does not support rate control\"}");
        return response;
    }
    
    if (mcs_rate < 0 || mcs_rate > 15) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"MCS rate must be between 0 and 15\"}");
        return response;
    }
    
    bool success = rubalink_radio_set_mcs_rate(interface_index, mcs_rate);
    if (success) {
        char success_json[256];
        snprintf(success_json, sizeof(success_json), 
                "{\"message\":\"MCS rate set to %d\",\"interface_index\":%d,\"mcs_rate\":%d}", 
                mcs_rate, interface_index, mcs_rate);
        rubalink_api_set_response(&response, 200, "OK", success_json);
    } else {
        rubalink_api_set_response(&response, 500, "Internal Server Error", "{\"error\":\"Failed to set MCS rate\"}");
    }
    
    return response;
}

rubalink_api_response_t rubalink_api_enable_radio_rate_control(int interface_index, bool enable) {
    rubalink_api_response_t response;
    
    if (interface_index < 0 || interface_index >= rubalink_radio_get_interfaces_count()) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Invalid interface index\"}");
        return response;
    }
    
    bool success = rubalink_radio_set_rate_control(interface_index, enable);
    if (success) {
        char success_json[256];
        snprintf(success_json, sizeof(success_json), 
                "{\"message\":\"Rate control %s\",\"interface_index\":%d,\"enabled\":%s}", 
                enable ? "enabled" : "disabled", interface_index, enable ? "true" : "false");
        rubalink_api_set_response(&response, 200, "OK", success_json);
    } else {
        rubalink_api_set_response(&response, 500, "Internal Server Error", "{\"error\":\"Failed to set rate control\"}");
    }
    
    return response;
}

rubalink_api_response_t rubalink_api_get_radio_signal_data(int interface_index) {
    rubalink_api_response_t response;
    
    if (interface_index < 0 || interface_index >= rubalink_radio_get_interfaces_count()) {
        rubalink_api_set_response(&response, 400, "Bad Request", "{\"error\":\"Invalid interface index\"}");
        return response;
    }
    
    rubalink_radio_interface_t* iface = rubalink_radio_get_interface(interface_index);
    if (iface) {
        char signal_json[256];
        snprintf(signal_json, sizeof(signal_json), 
                "{\"interface_index\":%d,\"rssi\":%d,\"dbm\":%d,\"snr\":%d,\"vlq\":%.1f,\"timestamp\":%lu}", 
                interface_index, iface->last_rssi, iface->last_dbm, iface->last_snr,
                rubalink_radio_calculate_vlq(interface_index),
                iface->last_update_time);
        rubalink_api_set_response(&response, 200, "OK", signal_json);
    } else {
        rubalink_api_set_response(&response, 500, "Internal Server Error", "{\"error\":\"Failed to read signal data\"}");
    }
    
    return response;
}
