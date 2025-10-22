#include "racing_mode_enhancements.h"
#include "advanced_signal_processing.h"
#include "enhanced_cooldown_control.h"
#include "qp_delta_config.h"
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static racing_mode_config_t s_RacingConfig = {
    .race_mode_enabled = false,
    .racing_fps = 120,
    .racing_video_resolution = "1280x720",
    .racing_exposure = 11,
    .racing_bitrate_max = 4,
    .racing_rssi_filter_chain = "1",  // Low-pass filter
    .racing_dbm_filter_chain = "1",   // Low-pass filter
    .racing_strict_cooldown_ms = 100,
    .racing_up_cooldown_ms = 1000,
    .racing_emergency_cooldown_ms = 25,
    .racing_qp_delta_low = 10,
    .racing_qp_delta_medium = 3,
    .racing_qp_delta_high = 0
};

static racing_mode_state_t s_RacingState = {
    .racing_mode_active = false,
    .racing_mode_transitioning = false,
    .original_fps = 60,
    .original_resolution = "1920x1080",
    .original_exposure = 15,
    .original_bitrate_max = 8
};

static unsigned long s_RacingModeStartTime = 0;
static int s_RacingModeTransitions = 0;

void racing_mode_init() {
    // Load configuration
    racing_mode_load_config();
    
    log_line("[AP-ALINK-FPV] Racing mode enhancements initialized");
    log_line("[AP-ALINK-FPV] Racing FPS: %d, Resolution: %s, Max Bitrate: %d Mbps", 
             s_RacingConfig.racing_fps, 
             s_RacingConfig.racing_video_resolution,
             s_RacingConfig.racing_bitrate_max);
}

void racing_mode_cleanup() {
    // Disable racing mode if active
    if (s_RacingState.racing_mode_active) {
        disable_racing_mode();
    }
    
    log_line("[AP-ALINK-FPV] Racing mode enhancements cleanup");
}

void enable_racing_mode() {
    if (s_RacingState.racing_mode_active || s_RacingState.racing_mode_transitioning) {
        return; // Already active or transitioning
    }
    
    s_RacingState.racing_mode_transitioning = true;
    transition_to_racing_mode();
    
    s_RacingState.racing_mode_active = true;
    s_RacingState.racing_mode_transitioning = false;
    s_RacingModeStartTime = get_current_time_ms();
    s_RacingModeTransitions++;
    
    log_line("[AP-ALINK-FPV] Racing mode ENABLED");
}

void disable_racing_mode() {
    if (!s_RacingState.racing_mode_active || s_RacingState.racing_mode_transitioning) {
        return; // Not active or transitioning
    }
    
    s_RacingState.racing_mode_transitioning = true;
    transition_from_racing_mode();
    
    s_RacingState.racing_mode_active = false;
    s_RacingState.racing_mode_transitioning = false;
    
    log_line("[AP-ALINK-FPV] Racing mode DISABLED");
}

bool is_racing_mode_enabled() {
    return s_RacingConfig.race_mode_enabled;
}

bool is_racing_mode_active() {
    return s_RacingState.racing_mode_active;
}

void racing_mode_save_config() {
    // Save configuration to RubyFPV config system
    log_line("[AP-ALINK-FPV] Racing mode config saved");
}

void racing_mode_load_config() {
    // Load configuration from RubyFPV config system
    log_line("[AP-ALINK-FPV] Racing mode config loaded");
}

void racing_mode_set_preset(const char* preset_name) {
    if (strcmp(preset_name, "ultra_racing") == 0) {
        // Ultra racing: Maximum performance
        s_RacingConfig.racing_fps = 144;
        strcpy(s_RacingConfig.racing_video_resolution, "1280x720");
        s_RacingConfig.racing_exposure = 8;
        s_RacingConfig.racing_bitrate_max = 6;
        s_RacingConfig.racing_strict_cooldown_ms = 50;
        s_RacingConfig.racing_up_cooldown_ms = 500;
        s_RacingConfig.racing_emergency_cooldown_ms = 10;
        log_line("[AP-ALINK-FPV] Applied ultra racing preset");
    }
    else if (strcmp(preset_name, "balanced_racing") == 0) {
        // Balanced racing: Good performance with stability
        s_RacingConfig.racing_fps = 120;
        strcpy(s_RacingConfig.racing_video_resolution, "1280x720");
        s_RacingConfig.racing_exposure = 11;
        s_RacingConfig.racing_bitrate_max = 4;
        s_RacingConfig.racing_strict_cooldown_ms = 100;
        s_RacingConfig.racing_up_cooldown_ms = 1000;
        s_RacingConfig.racing_emergency_cooldown_ms = 25;
        log_line("[AP-ALINK-FPV] Applied balanced racing preset");
    }
    else if (strcmp(preset_name, "conservative_racing") == 0) {
        // Conservative racing: Stability over performance
        s_RacingConfig.racing_fps = 90;
        strcpy(s_RacingConfig.racing_video_resolution, "1280x720");
        s_RacingConfig.racing_exposure = 15;
        s_RacingConfig.racing_bitrate_max = 3;
        s_RacingConfig.racing_strict_cooldown_ms = 200;
        s_RacingConfig.racing_up_cooldown_ms = 2000;
        s_RacingConfig.racing_emergency_cooldown_ms = 50;
        log_line("[AP-ALINK-FPV] Applied conservative racing preset");
    }
    else {
        log_line("[AP-ALINK-FPV] Unknown racing preset: %s", preset_name);
    }
}

float process_racing_rssi_signal(float raw_rssi) {
    // Use racing-specific filter chain for RSSI
    return process_rssi_signal(raw_rssi, true); // racing_mode = true
}

float process_racing_dbm_signal(float raw_dbm) {
    // Use racing-specific filter chain for dBm
    return process_dbm_signal(raw_dbm, true); // racing_mode = true
}

int calculate_racing_bitrate_change(int target_bitrate, int current_bitrate, float filtered_rssi, float filtered_dbm) {
    // Clamp target bitrate to racing maximum
    if (target_bitrate > s_RacingConfig.racing_bitrate_max) {
        target_bitrate = s_RacingConfig.racing_bitrate_max;
    }
    
    // Use racing-specific control algorithm (usually FIFO for faster response)
    return calculate_bitrate_adjustment(target_bitrate, current_bitrate, true); // racing_mode = true
}

void transition_to_racing_mode() {
    // Store original settings
    // TODO: Get these from RubyFPV's current settings
    s_RacingState.original_fps = 60;  // TODO: Get from RubyFPV
    strcpy(s_RacingState.original_resolution, "1920x1080");  // TODO: Get from RubyFPV
    s_RacingState.original_exposure = 15;  // TODO: Get from RubyFPV
    s_RacingState.original_bitrate_max = 8;  // TODO: Get from RubyFPV
    
    // Apply racing video settings
    apply_racing_video_settings();
    
    // Apply racing-specific QP delta settings
    // TODO: This would integrate with RubyFPV's QP delta system
    
    log_line("[AP-ALINK-FPV] Transitioned to racing mode");
}

void transition_from_racing_mode() {
    // Restore original video settings
    restore_original_video_settings();
    
    // Restore original QP delta settings
    // TODO: This would integrate with RubyFPV's QP delta system
    
    log_line("[AP-ALINK-FPV] Transitioned from racing mode");
}

void apply_racing_video_settings() {
    // Apply racing-specific video settings
    // TODO: Integrate with RubyFPV's video configuration system
    
    log_line("[AP-ALINK-FPV] Applied racing video settings: %dx%d @ %d FPS, exposure %d", 
             // Parse resolution string to get width and height
             s_RacingConfig.racing_fps, 
             s_RacingConfig.racing_exposure);
}

void restore_original_video_settings() {
    // Restore original video settings
    // TODO: Integrate with RubyFPV's video configuration system
    
    log_line("[AP-ALINK-FPV] Restored original video settings: %dx%d @ %d FPS, exposure %d", 
             s_RacingState.original_fps, 
             s_RacingState.original_exposure);
}

int get_racing_mode_duration_ms() {
    if (!s_RacingState.racing_mode_active) {
        return 0;
    }
    
    return get_current_time_ms() - s_RacingModeStartTime;
}

int get_racing_mode_transitions() {
    return s_RacingModeTransitions;
}

void reset_racing_mode_statistics() {
    s_RacingModeTransitions = 0;
    log_line("[AP-ALINK-FPV] Racing mode statistics reset");
}
