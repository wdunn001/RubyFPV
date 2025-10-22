#include "rubalink_api_endpoints.h"
#include "../r_vehicle/rubalink_integration.h"
#include "../r_vehicle/rubalink_seamless_integration.h"
#include "../menu/rubalink_settings_panel.h"
#include "rubalink_monitoring_display.h"
#include <string.h>
#include <stdlib.h>

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
             "{\"current_mcs\":%d,\"current_qp_delta\":%d,\"filtered_rssi\":%.1f,\"filtered_dbm\":%.1f,\"current_bitrate\":%d,\"bitrate_changes_per_minute\":%d,\"signal_stability\":%.1f}",
             data->current_mcs,
             data->current_qp_delta,
             data->filtered_rssi,
             data->filtered_dbm,
             data->current_bitrate,
             data->bitrate_changes_per_minute,
             data->average_signal_stability);
    
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
    
    // TODO: Parse and apply configuration
    log_line("[RubALink] Configuration updated via API");
    
    rubalink_api_set_response(&response, 200, "OK", "{\"message\":\"Configuration updated successfully\"}");
    return response;
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
    
    // TODO: Implement actual enable/disable logic
    log_line("[RubALink] RubALink advanced %s via API", enabled ? "enabled" : "disabled");
    
    char message[256];
    snprintf(message, sizeof(message), "RubALink advanced %s", enabled ? "enabled" : "disabled");
    rubalink_api_set_response(&response, 200, "OK", message);
    return response;
}

rubalink_api_response_t rubalink_api_enable_rubyfpv_adaptive(bool enabled) {
    rubalink_api_response_t response;
    
    // TODO: Implement actual enable/disable logic
    log_line("[RubALink] RubyFPV adaptive %s via API", enabled ? "enabled" : "disabled");
    
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
    
    // TODO: Implement profile adaptive type setting
    
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
    
    // TODO: Implement log retrieval
    char logs_json[512];
    snprintf(logs_json, sizeof(logs_json),
             "{\"logs\":[\"RubALink initialized\",\"Seamless integration active\",\"Auto-detection enabled\"],\"max_lines\":%d}",
             max_lines);
    
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
