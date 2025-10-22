#include "fallback_logic.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// Maximum number of conditions and actions
#define MAX_FALLBACK_CONDITIONS 10
#define MAX_FALLBACK_ACTIONS 5

// Global fallback system state
static fallback_system_state_t s_FallbackState = {0};
static fallback_config_t s_FallbackConfig = {
    .enable_fallback = true,
    .signal_loss_threshold = 20.0f,      // RSSI below 20%
    .signal_loss_duration_ms = 2000,     // 2 seconds
    .latency_threshold_ms = 100.0f,     // 100ms latency
    .latency_duration_ms = 1000,        // 1 second
    .packet_loss_threshold = 0.1f,       // 10% packet loss
    .packet_loss_duration_ms = 1500,     // 1.5 seconds
    .max_consecutive_failures = 5,       // 5 consecutive failures
    .auto_recovery_enabled = true,
    .recovery_delay_ms = 5000            // 5 seconds
};

// Active conditions and actions
static fallback_condition_t s_Conditions[MAX_FALLBACK_CONDITIONS];
static fallback_action_t s_Actions[MAX_FALLBACK_ACTIONS];
static uint8_t s_ConditionCount = 0;
static uint8_t s_ActionCount = 0;

void fallback_system_init() {
    memset(&s_FallbackState, 0, sizeof(s_FallbackState));
    memset(s_Conditions, 0, sizeof(s_Conditions));
    memset(s_Actions, 0, sizeof(s_Actions));
    
    s_ConditionCount = 0;
    s_ActionCount = 0;
    
    fallback_config_load();
    
    // Add default conditions
    fallback_add_condition(FALLBACK_CONDITION_SIGNAL_LOSS, 
                          s_FallbackConfig.signal_loss_threshold,
                          s_FallbackConfig.signal_loss_duration_ms);
    
    fallback_add_condition(FALLBACK_CONDITION_HIGH_LATENCY,
                          s_FallbackConfig.latency_threshold_ms,
                          s_FallbackConfig.latency_duration_ms);
    
    fallback_add_condition(FALLBACK_CONDITION_PACKET_LOSS,
                          s_FallbackConfig.packet_loss_threshold,
                          s_FallbackConfig.packet_loss_duration_ms);
    
    log_line("[Fallback] Fallback system initialized");
}

void fallback_system_cleanup() {
    memset(&s_FallbackState, 0, sizeof(s_FallbackState));
    memset(s_Conditions, 0, sizeof(s_Conditions));
    memset(s_Actions, 0, sizeof(s_Actions));
    
    s_ConditionCount = 0;
    s_ActionCount = 0;
    
    log_line("[Fallback] Fallback system cleaned up");
}

void fallback_system_update(float rssi, float dbm, float latency_ms, float packet_loss_rate) {
    if (!s_FallbackConfig.enable_fallback) {
        return;
    }
    
    uint32_t current_time = get_current_time_ms();
    s_FallbackState.last_check_time = current_time;
    
    // Check all conditions
    bool condition_triggered = fallback_system_check_conditions(rssi, dbm, latency_ms, packet_loss_rate);
    
    if (condition_triggered && !s_FallbackState.fallback_active) {
        // Start fallback sequence
        s_FallbackState.fallback_active = true;
        s_FallbackState.fallback_start_time = current_time;
        s_FallbackState.consecutive_failures++;
        
        log_line("[Fallback] Fallback activated due to condition: %s", 
                fallback_condition_type_to_string(s_FallbackState.active_condition));
        
        // Determine appropriate action
        fallback_action_type_t action = FALLBACK_ACTION_NONE;
        
        switch (s_FallbackState.active_condition) {
            case FALLBACK_CONDITION_SIGNAL_LOSS:
                action = FALLBACK_ACTION_SWITCH_TO_RUBYFPV;
                break;
            case FALLBACK_CONDITION_HIGH_LATENCY:
                action = FALLBACK_ACTION_REDUCE_BITRATE;
                break;
            case FALLBACK_CONDITION_PACKET_LOSS:
                action = FALLBACK_ACTION_RESET_FILTERS;
                break;
            case FALLBACK_CONDITION_SYSTEM_ERROR:
                action = FALLBACK_ACTION_RESTART_SYSTEM;
                break;
            case FALLBACK_CONDITION_PATTERN_MISMATCH:
                action = FALLBACK_ACTION_SWITCH_TO_RUBYFPV;
                break;
            default:
                action = FALLBACK_ACTION_SWITCH_TO_RUBYFPV;
                break;
        }
        
        fallback_system_execute_action(action);
    }
    else if (!condition_triggered && s_FallbackState.fallback_active) {
        // Check for recovery
        if (s_FallbackConfig.auto_recovery_enabled) {
            uint32_t fallback_duration = current_time - s_FallbackState.fallback_start_time;
            if (fallback_duration >= s_FallbackConfig.recovery_delay_ms) {
                log_line("[Fallback] Attempting automatic recovery");
                fallback_system_reset();
            }
        }
    }
    
    // Execute pending actions
    fallback_execute_pending_actions();
}

bool fallback_system_check_conditions(float rssi, float dbm, float latency_ms, float packet_loss_rate) {
    uint32_t current_time = get_current_time_ms();
    bool any_condition_active = false;
    
    for (uint8_t i = 0; i < s_ConditionCount; i++) {
        fallback_condition_t* condition = &s_Conditions[i];
        bool condition_met = false;
        
        switch (condition->type) {
            case FALLBACK_CONDITION_NONE:
                condition_met = false;
                break;
            case FALLBACK_CONDITION_SIGNAL_LOSS:
                condition_met = (rssi < condition->threshold);
                break;
            case FALLBACK_CONDITION_HIGH_LATENCY:
                condition_met = (latency_ms > condition->threshold);
                break;
            case FALLBACK_CONDITION_PACKET_LOSS:
                condition_met = (packet_loss_rate > condition->threshold);
                break;
            case FALLBACK_CONDITION_SYSTEM_ERROR:
                // This would be set by external system error detection
                condition_met = false; // Placeholder
                break;
            case FALLBACK_CONDITION_USER_REQUEST:
                condition_met = false; // Set by user action
                break;
            case FALLBACK_CONDITION_PATTERN_MISMATCH: {
                // Check if current pattern doesn't match system
                vehicle_pattern_detection_t detection = signal_pattern_analyze_vehicle_type();
                condition_met = (detection.confidence > 0.7f && 
                               detection.pattern_type == VEHICLE_PATTERN_RACING);
                break;
            }
            default:
                condition_met = false;
                break;
        }
        
        if (condition_met) {
            if (!condition->active) {
                condition->active = true;
                condition->trigger_time = current_time;
            }
            
            // Check if condition has persisted long enough
            uint32_t duration = current_time - condition->trigger_time;
            if (duration >= condition->duration_ms) {
                s_FallbackState.active_condition = condition->type;
                any_condition_active = true;
            }
        } else {
            condition->active = false;
        }
    }
    
    return any_condition_active;
}

void fallback_system_execute_action(fallback_action_type_t action) {
    if (s_FallbackState.user_override_active) {
        log_line("[Fallback] User override active, skipping action: %s", 
                fallback_action_type_to_string(action));
        return;
    }
    
    switch (action) {
        case FALLBACK_ACTION_SWITCH_TO_RUBYFPV:
            log_line("[Fallback] Switching to RubyFPV adaptive system");
            // This would integrate with RubyFPV's adaptive system
            break;
            
        case FALLBACK_ACTION_SWITCH_TO_RUBALINK:
            log_line("[Fallback] Switching to RubALink system");
            // This would integrate with RubALink system
            break;
            
        case FALLBACK_ACTION_REDUCE_BITRATE:
            log_line("[Fallback] Reducing bitrate for stability");
            // This would integrate with bitrate control
            break;
            
        case FALLBACK_ACTION_RESET_FILTERS:
            log_line("[Fallback] Resetting signal filters");
            // This would reset all signal processing filters
            break;
            
        case FALLBACK_ACTION_RESTART_SYSTEM:
            log_line("[Fallback] Restarting adaptive system");
            // This would restart the entire adaptive system
            break;
            
        default:
            log_line("[Fallback] Unknown action type: %d", action);
            break;
    }
    
    s_FallbackState.last_action = action;
}

void fallback_system_reset() {
    s_FallbackState.fallback_active = false;
    s_FallbackState.active_condition = FALLBACK_CONDITION_NONE;
    s_FallbackState.consecutive_failures = 0;
    
    // Clear all active conditions
    for (uint8_t i = 0; i < s_ConditionCount; i++) {
        s_Conditions[i].active = false;
    }
    
    log_line("[Fallback] Fallback system reset");
}

void fallback_config_load() {
    rubalink_config_get_bool("enable_fallback", &s_FallbackConfig.enable_fallback);
    rubalink_config_get_float("signal_loss_threshold", &s_FallbackConfig.signal_loss_threshold);
    rubalink_config_get_int("signal_loss_duration_ms", (int*)&s_FallbackConfig.signal_loss_duration_ms);
    rubalink_config_get_float("latency_threshold_ms", &s_FallbackConfig.latency_threshold_ms);
    rubalink_config_get_int("latency_duration_ms", (int*)&s_FallbackConfig.latency_duration_ms);
    rubalink_config_get_float("packet_loss_threshold", &s_FallbackConfig.packet_loss_threshold);
    rubalink_config_get_int("packet_loss_duration_ms", (int*)&s_FallbackConfig.packet_loss_duration_ms);
    rubalink_config_get_int("max_consecutive_failures", (int*)&s_FallbackConfig.max_consecutive_failures);
    rubalink_config_get_bool("auto_recovery_enabled", &s_FallbackConfig.auto_recovery_enabled);
    rubalink_config_get_int("recovery_delay_ms", (int*)&s_FallbackConfig.recovery_delay_ms);
}

void fallback_config_save() {
    rubalink_config_set_bool("enable_fallback", s_FallbackConfig.enable_fallback);
    rubalink_config_set_float("signal_loss_threshold", s_FallbackConfig.signal_loss_threshold);
    rubalink_config_set_int("signal_loss_duration_ms", s_FallbackConfig.signal_loss_duration_ms);
    rubalink_config_set_float("latency_threshold_ms", s_FallbackConfig.latency_threshold_ms);
    rubalink_config_set_int("latency_duration_ms", s_FallbackConfig.latency_duration_ms);
    rubalink_config_set_float("packet_loss_threshold", s_FallbackConfig.packet_loss_threshold);
    rubalink_config_set_int("packet_loss_duration_ms", s_FallbackConfig.packet_loss_duration_ms);
    rubalink_config_set_int("max_consecutive_failures", s_FallbackConfig.max_consecutive_failures);
    rubalink_config_set_bool("auto_recovery_enabled", s_FallbackConfig.auto_recovery_enabled);
    rubalink_config_set_int("recovery_delay_ms", s_FallbackConfig.recovery_delay_ms);
}

fallback_config_t fallback_get_config() {
    return s_FallbackConfig;
}

void fallback_set_config(const fallback_config_t* config) {
    s_FallbackConfig = *config;
    fallback_config_save();
}

bool fallback_add_condition(fallback_condition_type_t type, float threshold, uint32_t duration_ms) {
    if (s_ConditionCount >= MAX_FALLBACK_CONDITIONS) {
        return false;
    }
    
    // Check if condition already exists
    for (uint8_t i = 0; i < s_ConditionCount; i++) {
        if (s_Conditions[i].type == type) {
            // Update existing condition
            s_Conditions[i].threshold = threshold;
            s_Conditions[i].duration_ms = duration_ms;
            return true;
        }
    }
    
    // Add new condition
    fallback_condition_t* condition = &s_Conditions[s_ConditionCount];
    condition->type = type;
    condition->threshold = threshold;
    condition->duration_ms = duration_ms;
    condition->active = false;
    condition->trigger_time = 0;
    
    // Set description
    switch (type) {
        case FALLBACK_CONDITION_SIGNAL_LOSS:
            snprintf(condition->description, sizeof(condition->description), 
                    "Signal loss (RSSI < %.1f)", threshold);
            break;
        case FALLBACK_CONDITION_HIGH_LATENCY:
            snprintf(condition->description, sizeof(condition->description), 
                    "High latency (%.1f ms)", threshold);
            break;
        case FALLBACK_CONDITION_PACKET_LOSS:
            snprintf(condition->description, sizeof(condition->description), 
                    "Packet loss (%.1f%%)", threshold * 100.0f);
            break;
        default:
            snprintf(condition->description, sizeof(condition->description), 
                    "Condition type %d", type);
            break;
    }
    
    s_ConditionCount++;
    return true;
}

bool fallback_remove_condition(fallback_condition_type_t type) {
    for (uint8_t i = 0; i < s_ConditionCount; i++) {
        if (s_Conditions[i].type == type) {
            // Shift remaining conditions
            for (uint8_t j = i; j < s_ConditionCount - 1; j++) {
                s_Conditions[j] = s_Conditions[j + 1];
            }
            s_ConditionCount--;
            return true;
        }
    }
    return false;
}

bool fallback_condition_is_active(fallback_condition_type_t type) {
    for (uint8_t i = 0; i < s_ConditionCount; i++) {
        if (s_Conditions[i].type == type) {
            return s_Conditions[i].active;
        }
    }
    return false;
}

void fallback_clear_all_conditions() {
    s_ConditionCount = 0;
    memset(s_Conditions, 0, sizeof(s_Conditions));
}

bool fallback_add_action(fallback_action_type_t action, uint32_t delay_ms) {
    if (s_ActionCount >= MAX_FALLBACK_ACTIONS) {
        return false;
    }
    
    fallback_action_t* new_action = &s_Actions[s_ActionCount];
    new_action->type = action;
    new_action->delay_ms = delay_ms;
    new_action->executed = false;
    new_action->execution_time = 0;
    
    // Set description
    switch (action) {
        case FALLBACK_ACTION_SWITCH_TO_RUBYFPV:
            strcpy(new_action->description, "Switch to RubyFPV");
            break;
        case FALLBACK_ACTION_SWITCH_TO_RUBALINK:
            strcpy(new_action->description, "Switch to RubALink");
            break;
        case FALLBACK_ACTION_REDUCE_BITRATE:
            strcpy(new_action->description, "Reduce bitrate");
            break;
        case FALLBACK_ACTION_RESET_FILTERS:
            strcpy(new_action->description, "Reset filters");
            break;
        case FALLBACK_ACTION_RESTART_SYSTEM:
            strcpy(new_action->description, "Restart system");
            break;
        default:
            snprintf(new_action->description, sizeof(new_action->description), 
                    "Action type %d", action);
            break;
    }
    
    s_ActionCount++;
    return true;
}

void fallback_execute_pending_actions() {
    uint32_t current_time = get_current_time_ms();
    
    for (uint8_t i = 0; i < s_ActionCount; i++) {
        fallback_action_t* action = &s_Actions[i];
        
        if (!action->executed && 
            (current_time - s_FallbackState.fallback_start_time) >= action->delay_ms) {
            
            fallback_system_execute_action(action->type);
            action->executed = true;
            action->execution_time = current_time;
        }
    }
}

bool fallback_action_is_pending(fallback_action_type_t action) {
    for (uint8_t i = 0; i < s_ActionCount; i++) {
        if (s_Actions[i].type == action && !s_Actions[i].executed) {
            return true;
        }
    }
    return false;
}

bool fallback_is_active() {
    return s_FallbackState.fallback_active;
}

fallback_condition_type_t fallback_get_active_condition() {
    return s_FallbackState.active_condition;
}

fallback_action_type_t fallback_get_last_action() {
    return s_FallbackState.last_action;
}

uint32_t fallback_get_duration() {
    if (!s_FallbackState.fallback_active) {
        return 0;
    }
    return get_current_time_ms() - s_FallbackState.fallback_start_time;
}

uint8_t fallback_get_consecutive_failures() {
    return s_FallbackState.consecutive_failures;
}

void fallback_set_user_override(bool override) {
    s_FallbackState.user_override_active = override;
}

bool fallback_get_user_override() {
    return s_FallbackState.user_override_active;
}

void fallback_force_action(fallback_action_type_t action) {
    fallback_system_execute_action(action);
}

const char* fallback_condition_type_to_string(fallback_condition_type_t type) {
    switch (type) {
        case FALLBACK_CONDITION_SIGNAL_LOSS: return "Signal Loss";
        case FALLBACK_CONDITION_HIGH_LATENCY: return "High Latency";
        case FALLBACK_CONDITION_PACKET_LOSS: return "Packet Loss";
        case FALLBACK_CONDITION_SYSTEM_ERROR: return "System Error";
        case FALLBACK_CONDITION_USER_REQUEST: return "User Request";
        case FALLBACK_CONDITION_PATTERN_MISMATCH: return "Pattern Mismatch";
        default: return "Unknown";
    }
}

const char* fallback_action_type_to_string(fallback_action_type_t action) {
    switch (action) {
        case FALLBACK_ACTION_SWITCH_TO_RUBYFPV: return "Switch to RubyFPV";
        case FALLBACK_ACTION_SWITCH_TO_RUBALINK: return "Switch to RubALink";
        case FALLBACK_ACTION_REDUCE_BITRATE: return "Reduce Bitrate";
        case FALLBACK_ACTION_RESET_FILTERS: return "Reset Filters";
        case FALLBACK_ACTION_RESTART_SYSTEM: return "Restart System";
        default: return "Unknown";
    }
}

bool fallback_should_switch_system() {
    return s_FallbackState.fallback_active && 
           (s_FallbackState.last_action == FALLBACK_ACTION_SWITCH_TO_RUBYFPV ||
            s_FallbackState.last_action == FALLBACK_ACTION_SWITCH_TO_RUBALINK);
}

void fallback_log_status() {
    log_line("[Fallback] Status: %s", s_FallbackState.fallback_active ? "ACTIVE" : "INACTIVE");
    if (s_FallbackState.fallback_active) {
        log_line("[Fallback] Active condition: %s", 
                fallback_condition_type_to_string(s_FallbackState.active_condition));
        log_line("[Fallback] Last action: %s", 
                fallback_action_type_to_string(s_FallbackState.last_action));
        log_line("[Fallback] Duration: %u ms", fallback_get_duration());
        log_line("[Fallback] Consecutive failures: %u", s_FallbackState.consecutive_failures);
    }
}
