#pragma once

#include "../base/base.h"

// Enhanced Cooldown & Control Configuration
typedef struct {
    // Asymmetric cooldown timing
    unsigned long strict_cooldown_ms;      // Minimum time between any changes
    unsigned long up_cooldown_ms;          // Time before increasing bitrate
    unsigned long emergency_cooldown_ms;   // Time for emergency drops
    
    // Control parameters
    int min_change_percent;                // Minimum change percentage threshold
    int control_algorithm;                 // PID vs FIFO algorithm choice
    
    // PID parameters
    float pid_kp;                          // Proportional gain
    float pid_ki;                          // Integral gain
    float pid_kd;                          // Derivative gain
    
    // Emergency parameters
    int emergency_rssi_threshold;          // Emergency drop threshold
    int emergency_bitrate;                 // Emergency bitrate (kbps)
} enhanced_cooldown_config_t;

// Cooldown State
typedef struct {
    unsigned long last_change_time;        // Last bitrate change time
    unsigned long last_up_time;            // Last bitrate increase time
    int last_bitrate;                      // Last bitrate value
    bool emergency_mode;                   // Emergency mode flag
} cooldown_state_t;

// Function declarations
void enhanced_cooldown_init();
void enhanced_cooldown_cleanup();

// Cooldown logic functions
bool should_change_bitrate(int new_bitrate, int current_bitrate);
bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi);
void update_cooldown_timing(int new_bitrate, int current_bitrate);

// Configuration functions
void enhanced_cooldown_save_config();
void enhanced_cooldown_load_config();
void enhanced_cooldown_set_preset(const char* preset_name);

// Control algorithm functions
int calculate_bitrate_change(int target_bitrate, int current_bitrate, float filtered_rssi, float filtered_dbm);

// Emergency functions
void trigger_emergency_drop();
void exit_emergency_mode();
bool is_emergency_mode();

// Statistics functions
unsigned long get_time_since_last_change();
unsigned long get_time_since_last_increase();
int get_last_bitrate();
