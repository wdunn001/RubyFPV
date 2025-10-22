#pragma once

#include "../base/base.h"

// RubALink Adaptive Link Management Types
typedef enum {
    RUBALINK_ADAPTIVE_RUBYFPV = 0,    // RubyFPV's built-in adaptive video
    RUBALINK_ADAPTIVE_ADVANCED = 1,   // RubALink advanced system
    RUBALINK_ADAPTIVE_AUTO = 2        // Auto-detect based on vehicle profile
} rubalink_adaptive_type_t;

// Vehicle Profile Detection
typedef enum {
    VEHICLE_PROFILE_UNKNOWN = 0,
    VEHICLE_PROFILE_RACING = 1,      // Racing quad - use RubALink advanced
    VEHICLE_PROFILE_LONG_RANGE = 2,  // Long range - use RubALink advanced
    VEHICLE_PROFILE_CINEMATIC = 3,   // Cinematic - use RubyFPV adaptive
    VEHICLE_PROFILE_FREESTYLE = 4,   // Freestyle - use RubALink advanced
    VEHICLE_PROFILE_SURVEILLANCE = 5 // Surveillance - use RubyFPV adaptive
} vehicle_profile_type_t;

// RubALink Seamless Integration Configuration
typedef struct {
    // Global Settings
    bool rubalink_advanced_enabled;       // Enable RubALink advanced system
    bool rubyfpv_adaptive_enabled;        // Enable RubyFPV adaptive system
    bool auto_detection_enabled;          // Enable auto-detection
    
    // Profile-based Settings
    rubalink_adaptive_type_t racing_profile_adaptive;        // Racing vehicles
    rubalink_adaptive_type_t long_range_profile_adaptive;     // Long range vehicles
    rubalink_adaptive_type_t cinematic_profile_adaptive;     // Cinematic vehicles
    rubalink_adaptive_type_t freestyle_profile_adaptive;     // Freestyle vehicles
    rubalink_adaptive_type_t surveillance_profile_adaptive; // Surveillance vehicles
    rubalink_adaptive_type_t default_profile_adaptive;       // Default for unknown profiles
    
    // Auto-detection Settings
    bool detect_by_vehicle_name;        // Detect by vehicle name patterns
    bool detect_by_video_settings;     // Detect by video resolution/FPS
    bool detect_by_bitrate_patterns;   // Detect by bitrate usage patterns
    bool detect_by_signal_patterns;    // Detect by signal characteristics
    
    // Fallback Settings
    bool fallback_to_rubyfpv;          // Fallback to RubyFPV if RubALink fails
    bool fallback_to_rubalink;        // Fallback to RubALink if RubyFPV fails
    int fallback_timeout_ms;           // Timeout before fallback
    
    // User Override Settings
    bool allow_user_override;          // Allow user to override auto-detection
    bool remember_user_choice;         // Remember user's manual choice
} rubalink_seamless_config_t;

// Current Integration State
typedef struct {
    rubalink_adaptive_type_t current_adaptive_type;
    vehicle_profile_type_t detected_profile;
    bool auto_detection_active;
    bool user_override_active;
    unsigned long last_switch_time;
    int switch_count;
    bool fallback_active;
} rubalink_integration_state_t;

// Function declarations
void rubalink_seamless_init();
void rubalink_seamless_cleanup();

// Profile Detection Functions
vehicle_profile_type_t rubalink_detect_vehicle_profile(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate);
rubalink_adaptive_type_t rubalink_get_recommended_adaptive_type(vehicle_profile_type_t profile);
bool rubalink_is_profile_suitable_for_advanced(vehicle_profile_type_t profile);
bool rubalink_is_profile_suitable_for_rubyfpv(vehicle_profile_type_t profile);

// Integration Control Functions
void rubalink_seamless_activate_for_vehicle(const char* vehicle_name);
void rubalink_seamless_deactivate();
void rubalink_seamless_switch_to_advanced();
void rubalink_seamless_switch_to_rubyfpv();
void rubalink_seamless_switch_to_auto();

// Configuration Functions
void rubalink_seamless_load_config();
void rubalink_seamless_save_config();
void rubalink_seamless_set_profile_adaptive_type(vehicle_profile_type_t profile, rubalink_adaptive_type_t adaptive_type);
void rubalink_seamless_set_auto_detection_enabled(bool enabled);
void rubalink_seamless_set_fallback_settings(bool rubyfpv_fallback, bool rubalink_fallback, int timeout_ms);

// Status Functions
rubalink_adaptive_type_t rubalink_seamless_get_current_adaptive_type();
vehicle_profile_type_t rubalink_seamless_get_detected_profile();
bool rubalink_seamless_is_auto_detection_active();
bool rubalink_seamless_is_user_override_active();
rubalink_integration_state_t* rubalink_seamless_get_state();

// Auto-Detection Functions
void rubalink_seamless_update_vehicle_info(const char* vehicle_name, int video_width, int video_height, int fps, int max_bitrate);
void rubalink_seamless_analyze_signal_patterns(float avg_rssi, float avg_dbm, int bitrate_changes_per_minute);
void rubalink_seamless_analyze_video_patterns(int resolution_width, int resolution_height, int fps, int bitrate);

// Fallback Functions
void rubalink_seamless_check_fallback_conditions();
void rubalink_seamless_trigger_fallback();
bool rubalink_seamless_is_fallback_needed();

// User Override Functions
void rubalink_seamless_set_user_override(rubalink_adaptive_type_t adaptive_type);
void rubalink_seamless_clear_user_override();
bool rubalink_seamless_has_user_override();

// Settings Functions
void rubalink_seamless_enable_advanced(bool enabled);
void rubalink_seamless_enable_rubyfpv_adaptive(bool enabled);
void rubalink_seamless_enable_auto_detection(bool enabled);
void rubalink_seamless_set_detection_methods(bool by_name, bool by_video, bool by_bitrate, bool by_signal);

// Statistics Functions
void rubalink_seamless_reset_statistics();
int rubalink_seamless_get_switch_count();
unsigned long rubalink_seamless_get_last_switch_time();
void rubalink_seamless_get_statistics(char* stats_buffer, int max_length);
