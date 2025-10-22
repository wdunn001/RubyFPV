#include "rubalink_seamless_integration.h"
#include "../base/base.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static rubalink_seamless_config_t s_Config;
static rubalink_integration_state_t s_State;
static bool s_Initialized = false;

void rubalink_seamless_init() {
    log_line("[RubALink] Initializing seamless integration...");
    
    // Load default configuration
    rubalink_seamless_load_config();
    
    // Initialize state
    memset(&s_State, 0, sizeof(s_State));
    s_State.current_adaptive_type = RUBALINK_ADAPTIVE_AUTO;
    s_State.detected_profile = VEHICLE_PROFILE_UNKNOWN;
    s_State.auto_detection_active = s_Config.auto_detection_enabled;
    s_State.user_override_active = false;
    s_State.last_switch_time = 0;
    s_State.switch_count = 0;
    s_State.fallback_active = false;
    
    s_Initialized = true;
    log_line("[RubALink] Seamless integration initialized");
}

void rubalink_seamless_cleanup() {
    log_line("[RubALink] Cleaning up seamless integration...");
    s_Initialized = false;
}

vehicle_profile_type_t rubalink_detect_vehicle_profile(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate) {
    if (!vehicle_name) return VEHICLE_PROFILE_UNKNOWN;
    
    // Detect by vehicle name patterns
    if (s_Config.detect_by_vehicle_name) {
        char name_lower[256];
        strncpy(name_lower, vehicle_name, sizeof(name_lower) - 1);
        name_lower[sizeof(name_lower) - 1] = '\0';
        
        // Convert to lowercase for pattern matching
        for (int i = 0; name_lower[i]; i++) {
            if (name_lower[i] >= 'A' && name_lower[i] <= 'Z') {
                name_lower[i] = name_lower[i] - 'A' + 'a';
            }
        }
        
        if (strstr(name_lower, "race") || strstr(name_lower, "racing") || strstr(name_lower, "speed")) {
            return VEHICLE_PROFILE_RACING;
        }
        if (strstr(name_lower, "long") || strstr(name_lower, "range") || strstr(name_lower, "lr")) {
            return VEHICLE_PROFILE_LONG_RANGE;
        }
        if (strstr(name_lower, "cine") || strstr(name_lower, "cinematic") || strstr(name_lower, "movie")) {
            return VEHICLE_PROFILE_CINEMATIC;
        }
        if (strstr(name_lower, "free") || strstr(name_lower, "freestyle") || strstr(name_lower, "acro")) {
            return VEHICLE_PROFILE_FREESTYLE;
        }
        if (strstr(name_lower, "surv") || strstr(name_lower, "surveillance") || strstr(name_lower, "monitor")) {
            return VEHICLE_PROFILE_SURVEILLANCE;
        }
    }
    
    // Detect by video settings
    if (s_Config.detect_by_video_settings) {
        // High FPS and lower resolution suggests racing
        if (fps >= 120 && video_width <= 1280) {
            return VEHICLE_PROFILE_RACING;
        }
        
        // High resolution and lower FPS suggests cinematic
        if (video_width >= 1920 && fps <= 60) {
            return VEHICLE_PROFILE_CINEMATIC;
        }
        
        // Medium settings suggest freestyle
        if (fps >= 60 && fps < 120 && video_width >= 1280 && video_width < 1920) {
            return VEHICLE_PROFILE_FREESTYLE;
        }
    }
    
    // Detect by bitrate patterns
    if (s_Config.detect_by_bitrate_patterns) {
        // Low bitrate suggests long range
        if (max_bitrate <= 2) {
            return VEHICLE_PROFILE_LONG_RANGE;
        }
        
        // High bitrate suggests racing or freestyle
        if (max_bitrate >= 8) {
            return VEHICLE_PROFILE_RACING;
        }
    }
    
    return VEHICLE_PROFILE_UNKNOWN;
}

rubalink_adaptive_type_t rubalink_get_recommended_adaptive_type(vehicle_profile_type_t profile) {
    switch (profile) {
        case VEHICLE_PROFILE_RACING:
            return s_Config.racing_profile_adaptive;
        case VEHICLE_PROFILE_LONG_RANGE:
            return s_Config.long_range_profile_adaptive;
        case VEHICLE_PROFILE_CINEMATIC:
            return s_Config.cinematic_profile_adaptive;
        case VEHICLE_PROFILE_FREESTYLE:
            return s_Config.freestyle_profile_adaptive;
        case VEHICLE_PROFILE_SURVEILLANCE:
            return s_Config.surveillance_profile_adaptive;
        default:
            return s_Config.default_profile_adaptive;
    }
}

bool rubalink_is_profile_suitable_for_advanced(vehicle_profile_type_t profile) {
    rubalink_adaptive_type_t recommended = rubalink_get_recommended_adaptive_type(profile);
    return (recommended == RUBALINK_ADAPTIVE_ADVANCED || recommended == RUBALINK_ADAPTIVE_AUTO);
}

bool rubalink_is_profile_suitable_for_rubyfpv(vehicle_profile_type_t profile) {
    rubalink_adaptive_type_t recommended = rubalink_get_recommended_adaptive_type(profile);
    return (recommended == RUBALINK_ADAPTIVE_RUBYFPV || recommended == RUBALINK_ADAPTIVE_AUTO);
}

void rubalink_seamless_activate_for_vehicle(const char* vehicle_name) {
    if (!vehicle_name) return;
    
    log_line("[RubALink] Activating seamless integration for vehicle: %s", vehicle_name);
    
    // Detect vehicle profile
    vehicle_profile_type_t profile = rubalink_detect_vehicle_profile(vehicle_name, 1280, 720, 60, 4);
    s_State.detected_profile = profile;
    
    // Get recommended adaptive type
    rubalink_adaptive_type_t recommended = rubalink_get_recommended_adaptive_type(profile);
    
    // Apply auto-detection or user override
    if (s_State.auto_detection_active && !s_State.user_override_active) {
        s_State.current_adaptive_type = recommended;
    }
    
    log_line("[RubALink] Vehicle profile: %d, Adaptive type: %d", profile, s_State.current_adaptive_type);
}

void rubalink_seamless_deactivate() {
    log_line("[RubALink] Deactivating seamless integration");
    s_State.current_adaptive_type = RUBALINK_ADAPTIVE_AUTO;
    s_State.detected_profile = VEHICLE_PROFILE_UNKNOWN;
}

void rubalink_seamless_switch_to_advanced() {
    log_line("[RubALink] Switching to RubALink advanced");
    s_State.current_adaptive_type = RUBALINK_ADAPTIVE_ADVANCED;
    s_State.user_override_active = true;
    s_State.last_switch_time = get_current_time_ms();
    s_State.switch_count++;
}

void rubalink_seamless_switch_to_rubyfpv() {
    log_line("[RubALink] Switching to RubyFPV adaptive");
    s_State.current_adaptive_type = RUBALINK_ADAPTIVE_RUBYFPV;
    s_State.user_override_active = true;
    s_State.last_switch_time = get_current_time_ms();
    s_State.switch_count++;
}

void rubalink_seamless_switch_to_auto() {
    log_line("[RubALink] Switching to auto-detection");
    s_State.current_adaptive_type = RUBALINK_ADAPTIVE_AUTO;
    s_State.user_override_active = false;
    s_State.last_switch_time = get_current_time_ms();
    s_State.switch_count++;
}

void rubalink_seamless_load_config() {
    log_line("[RubALink] Loading seamless integration config...");
    
    // Load default configuration
    s_Config.rubalink_advanced_enabled = true;
    s_Config.rubyfpv_adaptive_enabled = true;
    s_Config.auto_detection_enabled = true;
    
    s_Config.racing_profile_adaptive = RUBALINK_ADAPTIVE_ADVANCED;
    s_Config.long_range_profile_adaptive = RUBALINK_ADAPTIVE_ADVANCED;
    s_Config.cinematic_profile_adaptive = RUBALINK_ADAPTIVE_RUBYFPV;
    s_Config.freestyle_profile_adaptive = RUBALINK_ADAPTIVE_ADVANCED;
    s_Config.surveillance_profile_adaptive = RUBALINK_ADAPTIVE_RUBYFPV;
    s_Config.default_profile_adaptive = RUBALINK_ADAPTIVE_AUTO;
    
    s_Config.detect_by_vehicle_name = true;
    s_Config.detect_by_video_settings = true;
    s_Config.detect_by_bitrate_patterns = true;
    s_Config.detect_by_signal_patterns = true;
    
    s_Config.fallback_to_rubyfpv = true;
    s_Config.fallback_to_rubalink = true;
    s_Config.fallback_timeout_ms = 5000;
    
    s_Config.allow_user_override = true;
    s_Config.remember_user_choice = true;
    
    // TODO: Load from actual config file
}

void rubalink_seamless_save_config() {
    log_line("[RubALink] Saving seamless integration config...");
    // TODO: Save to actual config file
}

void rubalink_seamless_set_profile_adaptive_type(vehicle_profile_type_t profile, rubalink_adaptive_type_t adaptive_type) {
    switch (profile) {
        case VEHICLE_PROFILE_RACING:
            s_Config.racing_profile_adaptive = adaptive_type;
            break;
        case VEHICLE_PROFILE_LONG_RANGE:
            s_Config.long_range_profile_adaptive = adaptive_type;
            break;
        case VEHICLE_PROFILE_CINEMATIC:
            s_Config.cinematic_profile_adaptive = adaptive_type;
            break;
        case VEHICLE_PROFILE_FREESTYLE:
            s_Config.freestyle_profile_adaptive = adaptive_type;
            break;
        case VEHICLE_PROFILE_SURVEILLANCE:
            s_Config.surveillance_profile_adaptive = adaptive_type;
            break;
        default:
            s_Config.default_profile_adaptive = adaptive_type;
            break;
    }
    log_line("[RubALink] Profile %d set to adaptive type %d", profile, adaptive_type);
}

void rubalink_seamless_set_auto_detection_enabled(bool enabled) {
    s_Config.auto_detection_enabled = enabled;
    s_State.auto_detection_active = enabled;
    log_line("[RubALink] Auto-detection %s", enabled ? "enabled" : "disabled");
}

void rubalink_seamless_set_fallback_settings(bool rubyfpv_fallback, bool rubalink_fallback, int timeout_ms) {
    s_Config.fallback_to_rubyfpv = rubyfpv_fallback;
    s_Config.fallback_to_rubalink = rubalink_fallback;
    s_Config.fallback_timeout_ms = timeout_ms;
    log_line("[RubALink] Fallback settings updated");
}

rubalink_adaptive_type_t rubalink_seamless_get_current_adaptive_type() {
    return s_State.current_adaptive_type;
}

vehicle_profile_type_t rubalink_seamless_get_detected_profile() {
    return s_State.detected_profile;
}

bool rubalink_seamless_is_auto_detection_active() {
    return s_State.auto_detection_active;
}

bool rubalink_seamless_is_user_override_active() {
    return s_State.user_override_active;
}

rubalink_integration_state_t* rubalink_seamless_get_state() {
    return &s_State;
}

void rubalink_seamless_update_vehicle_info(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate) {
    if (!vehicle_name) return;
    
    vehicle_profile_type_t new_profile = rubalink_detect_vehicle_profile(vehicle_name, video_width, video_height, fps, max_bitrate);
    
    if (new_profile != s_State.detected_profile) {
        s_State.detected_profile = new_profile;
        
        if (s_State.auto_detection_active && !s_State.user_override_active) {
            rubalink_adaptive_type_t recommended = rubalink_get_recommended_adaptive_type(new_profile);
            if (recommended != s_State.current_adaptive_type) {
                s_State.current_adaptive_type = recommended;
                s_State.last_switch_time = get_current_time_ms();
                s_State.switch_count++;
                log_line("[RubALink] Auto-switched to adaptive type %d for profile %d", recommended, new_profile);
            }
        }
    }
}

void rubalink_seamless_analyze_signal_patterns(float avg_rssi, float avg_dbm, int bitrate_changes_per_minute) {
    // TODO: Implement signal pattern analysis
    (void)avg_rssi;
    (void)avg_dbm;
    (void)bitrate_changes_per_minute;
}

void rubalink_seamless_analyze_video_patterns(int resolution_width, int resolution_height, int fps, int bitrate) {
    // TODO: Implement video pattern analysis
    (void)resolution_width;
    (void)resolution_height;
    (void)fps;
    (void)bitrate;
}

void rubalink_seamless_check_fallback_conditions() {
    // TODO: Implement fallback condition checking
}

void rubalink_seamless_trigger_fallback() {
    log_line("[RubALink] Triggering fallback");
    s_State.fallback_active = true;
    // TODO: Implement fallback logic
}

bool rubalink_seamless_is_fallback_needed() {
    return s_State.fallback_active;
}

void rubalink_seamless_set_user_override(rubalink_adaptive_type_t adaptive_type) {
    s_State.current_adaptive_type = adaptive_type;
    s_State.user_override_active = true;
    s_State.last_switch_time = get_current_time_ms();
    s_State.switch_count++;
    log_line("[RubALink] User override set to adaptive type %d", adaptive_type);
}

void rubalink_seamless_clear_user_override() {
    s_State.user_override_active = false;
    log_line("[RubALink] User override cleared");
}

bool rubalink_seamless_has_user_override() {
    return s_State.user_override_active;
}

void rubalink_seamless_enable_advanced(bool enabled) {
    s_Config.rubalink_advanced_enabled = enabled;
    log_line("[RubALink] RubALink advanced %s", enabled ? "enabled" : "disabled");
}

void rubalink_seamless_enable_rubyfpv_adaptive(bool enabled) {
    s_Config.rubyfpv_adaptive_enabled = enabled;
    log_line("[RubALink] RubyFPV adaptive %s", enabled ? "enabled" : "disabled");
}

void rubalink_seamless_enable_auto_detection(bool enabled) {
    s_Config.auto_detection_enabled = enabled;
    s_State.auto_detection_active = enabled;
    log_line("[RubALink] Auto-detection %s", enabled ? "enabled" : "disabled");
}

void rubalink_seamless_set_detection_methods(bool by_name, bool by_video, bool by_bitrate, bool by_signal) {
    s_Config.detect_by_vehicle_name = by_name;
    s_Config.detect_by_video_settings = by_video;
    s_Config.detect_by_bitrate_patterns = by_bitrate;
    s_Config.detect_by_signal_patterns = by_signal;
    log_line("[RubALink] Detection methods updated");
}

void rubalink_seamless_reset_statistics() {
    s_State.switch_count = 0;
    s_State.last_switch_time = 0;
    s_State.fallback_active = false;
    log_line("[RubALink] Statistics reset");
}

int rubalink_seamless_get_switch_count() {
    return s_State.switch_count;
}

unsigned long rubalink_seamless_get_last_switch_time() {
    return s_State.last_switch_time;
}

void rubalink_seamless_get_statistics(char* stats_buffer, int max_length) {
    if (!stats_buffer || max_length <= 0) return;
    
    snprintf(stats_buffer, max_length,
             "RubALink Stats: Switches=%d, LastSwitch=%lu, Profile=%d, Adaptive=%d, AutoDetect=%s, UserOverride=%s",
             s_State.switch_count,
             s_State.last_switch_time,
             s_State.detected_profile,
             s_State.current_adaptive_type,
             s_State.auto_detection_active ? "Yes" : "No",
             s_State.user_override_active ? "Yes" : "No");
}
