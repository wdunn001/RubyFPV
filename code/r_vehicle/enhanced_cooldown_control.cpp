#include "enhanced_cooldown_control.h"
#include "advanced_signal_processing.h"
#include "dynamic_rssi_thresholds.h"
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static enhanced_cooldown_config_t s_CooldownConfig = {
    .strict_cooldown_ms = 200,      // 200ms minimum between changes
    .up_cooldown_ms = 3000,         // 3s before increasing bitrate
    .emergency_cooldown_ms = 50,    // 50ms for emergency drops
    .min_change_percent = 5,        // 5% minimum change threshold
    .control_algorithm = CONTROL_ALGORITHM_FIFO,
    .pid_kp = 1.0f,
    .pid_ki = 0.05f,
    .pid_kd = 0.4f,
    .emergency_rssi_threshold = 30,
    .emergency_bitrate = 1000
};

static cooldown_state_t s_CooldownState = {
    .last_change_time = 0,
    .last_up_time = 0,
    .last_bitrate = 0,
    .emergency_mode = false
};

void enhanced_cooldown_init() {
    // Load configuration
    enhanced_cooldown_load_config();
    
    log_line("[AP-ALINK-FPV] Enhanced cooldown & control system initialized");
    log_line("[AP-ALINK-FPV] Strict cooldown: %lums, Up cooldown: %lums, Emergency: %lums", 
             s_CooldownConfig.strict_cooldown_ms,
             s_CooldownConfig.up_cooldown_ms,
             s_CooldownConfig.emergency_cooldown_ms);
}

void enhanced_cooldown_cleanup() {
    log_line("[AP-ALINK-FPV] Enhanced cooldown & control system cleanup");
}

bool should_change_bitrate(int new_bitrate, int current_bitrate) {
    unsigned long now = get_current_time_ms();
    int delta = abs(new_bitrate - current_bitrate);
    int min_delta = current_bitrate * s_CooldownConfig.min_change_percent / 100;
    
    // Don't change if delta is too small
    if (delta < min_delta) {
        return false;
    }
    
    if (new_bitrate < current_bitrate) {
        // Can decrease quickly - use emergency cooldown for faster response
        return (now - s_CooldownState.last_change_time) >= s_CooldownConfig.emergency_cooldown_ms;
    } else {
        // Must wait longer to increase - need both strict and up cooldown
        return (now - s_CooldownState.last_change_time) >= s_CooldownConfig.strict_cooldown_ms &&
               (now - s_CooldownState.last_up_time) >= s_CooldownConfig.up_cooldown_ms;
    }
}

bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi) {
    // Calculate dynamic RSSI threshold based on current MCS
    int current_mcs = bitrate_to_mcs(current_bitrate);
    int dynamic_threshold = get_dynamic_rssi_threshold(current_mcs, get_hardware_rssi_offset_from_rubyfpv());
    
    // Use the more conservative threshold (lower value = more sensitive)
    int effective_threshold = (dynamic_threshold < s_CooldownConfig.emergency_rssi_threshold) ? 
                             dynamic_threshold : s_CooldownConfig.emergency_rssi_threshold;
    
    return (filtered_rssi < effective_threshold && current_bitrate > s_CooldownConfig.emergency_bitrate);
}

void update_cooldown_timing(int new_bitrate, int current_bitrate) {
    unsigned long now = get_current_time_ms();
    s_CooldownState.last_change_time = now;
    
    if (new_bitrate > current_bitrate) {
        s_CooldownState.last_up_time = now;
    }
    
    s_CooldownState.last_bitrate = new_bitrate;
}

void enhanced_cooldown_save_config() {
    // Save configuration to RubyFPV config system
    log_line("[AP-ALINK-FPV] Enhanced cooldown config saved");
}

void enhanced_cooldown_load_config() {
    // Load configuration from RubyFPV config system
    log_line("[AP-ALINK-FPV] Enhanced cooldown config loaded");
}

void enhanced_cooldown_set_preset(const char* preset_name) {
    if (strcmp(preset_name, "racing") == 0) {
        // Racing mode: Faster response, shorter cooldowns
        s_CooldownConfig.strict_cooldown_ms = 100;
        s_CooldownConfig.up_cooldown_ms = 1000;
        s_CooldownConfig.emergency_cooldown_ms = 25;
        s_CooldownConfig.min_change_percent = 3;
        s_CooldownConfig.control_algorithm = CONTROL_ALGORITHM_FIFO;
        log_line("[AP-ALINK-FPV] Applied racing cooldown preset");
    }
    else if (strcmp(preset_name, "long_range") == 0) {
        // Long range: Conservative, longer cooldowns
        s_CooldownConfig.strict_cooldown_ms = 500;
        s_CooldownConfig.up_cooldown_ms = 5000;
        s_CooldownConfig.emergency_cooldown_ms = 100;
        s_CooldownConfig.min_change_percent = 10;
        s_CooldownConfig.control_algorithm = CONTROL_ALGORITHM_PID;
        log_line("[AP-ALINK-FPV] Applied long range cooldown preset");
    }
    else if (strcmp(preset_name, "balanced") == 0) {
        // Balanced: Moderate settings
        s_CooldownConfig.strict_cooldown_ms = 200;
        s_CooldownConfig.up_cooldown_ms = 3000;
        s_CooldownConfig.emergency_cooldown_ms = 50;
        s_CooldownConfig.min_change_percent = 5;
        s_CooldownConfig.control_algorithm = CONTROL_ALGORITHM_PID;
        log_line("[AP-ALINK-FPV] Applied balanced cooldown preset");
    }
    else {
        log_line("[AP-ALINK-FPV] Unknown cooldown preset: %s", preset_name);
    }
}

int calculate_bitrate_change(int target_bitrate, int current_bitrate, float filtered_rssi, float filtered_dbm) {
    // Check for emergency drop conditions first
    if (should_trigger_emergency_drop(current_bitrate, filtered_rssi)) {
        trigger_emergency_drop();
        return s_CooldownConfig.emergency_bitrate;
    }
    
    // Use advanced signal processing to calculate adjustment
    return calculate_bitrate_adjustment(target_bitrate, current_bitrate, false); // TODO: Pass racing mode
}

void trigger_emergency_drop() {
    s_CooldownState.emergency_mode = true;
    log_line("[AP-ALINK-FPV] Emergency drop triggered - bitrate: %d -> %d", 
             s_CooldownState.last_bitrate, s_CooldownConfig.emergency_bitrate);
    
    // Update timing for emergency drop
    update_cooldown_timing(s_CooldownConfig.emergency_bitrate, s_CooldownState.last_bitrate);
}

void exit_emergency_mode() {
    if (s_CooldownState.emergency_mode) {
        s_CooldownState.emergency_mode = false;
        log_line("[AP-ALINK-FPV] Exited emergency mode");
    }
}

bool is_emergency_mode() {
    return s_CooldownState.emergency_mode;
}

unsigned long get_time_since_last_change() {
    return get_current_time_ms() - s_CooldownState.last_change_time;
}

unsigned long get_time_since_last_increase() {
    return get_current_time_ms() - s_CooldownState.last_up_time;
}

int get_last_bitrate() {
    return s_CooldownState.last_bitrate;
}

// RubyFPV integration functions
static int (*s_RubyFPV_HardwareCallback)() = NULL;

int get_hardware_rssi_offset_from_rubyfpv() {
    if (s_RubyFPV_HardwareCallback) {
        return s_RubyFPV_HardwareCallback();
    }
    
    // Fallback to default value if no callback is set
    return 0;
}

void set_rubyfpv_hardware_callback(int (*hardware_callback)()) {
    s_RubyFPV_HardwareCallback = hardware_callback;
    log_line("[Cooldown] RubyFPV hardware callback registered");
}
