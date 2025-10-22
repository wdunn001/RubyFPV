#pragma once

#include "../base/base.h"
#include "../base/rubalink_config.h"
#include "signal_pattern_analysis.h"
#include <stdint.h>
#include <stdbool.h>

// Fallback condition types
typedef enum {
    FALLBACK_CONDITION_NONE = 0,
    FALLBACK_CONDITION_SIGNAL_LOSS,      // Signal quality below threshold
    FALLBACK_CONDITION_HIGH_LATENCY,     // Latency above threshold
    FALLBACK_CONDITION_PACKET_LOSS,      // Packet loss above threshold
    FALLBACK_CONDITION_SYSTEM_ERROR,     // System error detected
    FALLBACK_CONDITION_USER_REQUEST,     // User requested fallback
    FALLBACK_CONDITION_PATTERN_MISMATCH  // Detected pattern doesn't match current system
} fallback_condition_type_t;

// Fallback action types
typedef enum {
    FALLBACK_ACTION_NONE = 0,
    FALLBACK_ACTION_SWITCH_TO_RUBYFPV,   // Switch to RubyFPV adaptive
    FALLBACK_ACTION_SWITCH_TO_RUBALINK,  // Switch to RubALink
    FALLBACK_ACTION_REDUCE_BITRATE,      // Reduce bitrate
    FALLBACK_ACTION_RESET_FILTERS,       // Reset signal filters
    FALLBACK_ACTION_RESTART_SYSTEM       // Restart adaptive system
} fallback_action_type_t;

// Fallback condition structure
typedef struct {
    fallback_condition_type_t type;
    float threshold;              // Threshold value for condition
    uint32_t duration_ms;        // Duration condition must persist
    uint32_t trigger_time;       // When condition was triggered
    bool active;                 // Whether condition is currently active
    char description[64];         // Human-readable description
} fallback_condition_t;

// Fallback action structure
typedef struct {
    fallback_action_type_t type;
    uint32_t delay_ms;           // Delay before executing action
    bool executed;               // Whether action has been executed
    uint32_t execution_time;     // When action was executed
    char description[64];         // Human-readable description
} fallback_action_t;

// Fallback system state
typedef struct {
    bool fallback_active;        // Whether fallback is currently active
    fallback_condition_type_t active_condition;  // Currently active condition
    fallback_action_type_t last_action;         // Last executed action
    uint32_t fallback_start_time; // When fallback was initiated
    uint32_t last_check_time;    // Last time conditions were checked
    uint8_t consecutive_failures; // Number of consecutive failures
    bool user_override_active;   // Whether user has overridden fallback
} fallback_system_state_t;

// Fallback configuration
typedef struct {
    bool enable_fallback;         // Enable fallback system
    float signal_loss_threshold; // RSSI threshold for signal loss
    uint32_t signal_loss_duration_ms; // Duration for signal loss condition
    float latency_threshold_ms;  // Latency threshold
    uint32_t latency_duration_ms; // Duration for latency condition
    float packet_loss_threshold; // Packet loss threshold (0-1)
    uint32_t packet_loss_duration_ms; // Duration for packet loss condition
    uint32_t max_consecutive_failures; // Max failures before system restart
    bool auto_recovery_enabled;  // Enable automatic recovery
    uint32_t recovery_delay_ms;  // Delay before attempting recovery
} fallback_config_t;

// Function declarations
void fallback_system_init();
void fallback_system_cleanup();
void fallback_system_update(float rssi, float dbm, float latency_ms, float packet_loss_rate);
bool fallback_system_check_conditions(float rssi, float dbm, float latency_ms, float packet_loss_rate);
void fallback_system_execute_action(fallback_action_type_t action);
void fallback_system_reset();

// Configuration functions
void fallback_config_load();
void fallback_config_save();
fallback_config_t fallback_get_config();
void fallback_set_config(const fallback_config_t* config);

// Condition management
bool fallback_add_condition(fallback_condition_type_t type, float threshold, uint32_t duration_ms);
bool fallback_remove_condition(fallback_condition_type_t type);
bool fallback_condition_is_active(fallback_condition_type_t type);
void fallback_clear_all_conditions();

// Action management
bool fallback_add_action(fallback_action_type_t action, uint32_t delay_ms);
void fallback_execute_pending_actions();
bool fallback_action_is_pending(fallback_action_type_t action);

// State queries
bool fallback_is_active();
fallback_condition_type_t fallback_get_active_condition();
fallback_action_type_t fallback_get_last_action();
uint32_t fallback_get_duration();
uint8_t fallback_get_consecutive_failures();

// User control
void fallback_set_user_override(bool override);
bool fallback_get_user_override();
void fallback_force_action(fallback_action_type_t action);

// Utility functions
const char* fallback_condition_type_to_string(fallback_condition_type_t type);
const char* fallback_action_type_to_string(fallback_action_type_t action);
bool fallback_should_switch_system();
void fallback_log_status();
