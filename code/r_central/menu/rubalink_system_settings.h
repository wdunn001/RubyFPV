#pragma once

#include "../../base/base.h"
#include "../../r_vehicle/rubalink_seamless_integration.h"

// RubALink System Settings
typedef struct {
    // System Enable/Disable Toggles
    bool rubalink_advanced_enabled;              // Enable RubALink advanced system
    bool rubyfpv_adaptive_enabled;               // Enable RubyFPV adaptive system
    bool auto_detection_enabled;                 // Enable auto-detection
    
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
    
    // Auto-search Settings
    bool disable_rubalink_in_auto_search;    // Disable RubALink in auto searches
    bool disable_rubyfpv_in_auto_search;     // Disable RubyFPV in auto searches
    bool prefer_rubalink_for_new_vehicles;   // Prefer RubALink for new vehicles
    bool prefer_rubyfpv_for_new_vehicles;    // Prefer RubyFPV for new vehicles
} rubalink_system_settings_t;

// Function declarations
void rubalink_system_settings_init();
void rubalink_system_settings_cleanup();
void rubalink_system_settings_show();
void rubalink_system_settings_hide();
bool rubalink_system_settings_is_active();

// Settings management
void rubalink_system_settings_load_defaults();
void rubalink_system_settings_load_from_config();
void rubalink_system_settings_save_to_config();
void rubalink_system_settings_reset_to_defaults();

// UI control functions
void rubalink_system_settings_render();
void rubalink_system_settings_handle_input(int key);
void rubalink_system_settings_navigate_up();
void rubalink_system_settings_navigate_down();
void rubalink_system_settings_toggle_boolean();
void rubalink_system_settings_change_adaptive_type();

// System control functions
void rubalink_system_settings_enable_rubalink_advanced(bool enabled);
void rubalink_system_settings_enable_rubyfpv_adaptive(bool enabled);
void rubalink_system_settings_enable_auto_detection(bool enabled);
void rubalink_system_settings_set_profile_adaptive_type(vehicle_profile_type_t profile, rubalink_adaptive_type_t adaptive_type);

// Auto-search control functions
void rubalink_system_settings_set_auto_search_preferences(bool disable_rubalink, bool disable_rubyfpv, bool prefer_rubalink, bool prefer_rubyfpv);
bool rubalink_system_settings_should_disable_rubalink_in_auto_search();
bool rubalink_system_settings_should_disable_rubyfpv_in_auto_search();
bool rubalink_system_settings_should_prefer_rubalink_for_new_vehicles();
bool rubalink_system_settings_should_prefer_rubyfpv_for_new_vehicles();

// Status functions
rubalink_system_settings_t* rubalink_system_settings_get_current();
void rubalink_system_settings_set_current(const rubalink_system_settings_t* settings);
bool rubalink_system_settings_has_unsaved_changes();
void rubalink_system_settings_mark_saved();

// Validation functions
bool rubalink_system_settings_validate();
void rubalink_system_settings_validate_and_correct();
const char* rubalink_system_settings_get_validation_error();
