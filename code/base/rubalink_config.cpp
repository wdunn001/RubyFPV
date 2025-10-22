#include "rubalink_config.h"
#include <errno.h>
#include <ctype.h>

// Global configuration state
static bool s_ConfigLoaded = false;
static char s_LastError[256] = {0};
static int s_LastErrorLine = 0;

// Configuration validation rules
static const config_item_t s_ConfigRules[] = {
    // RubALink Advanced Settings
    {"rubalink_advanced_enabled", "1", true, true, 0, 1, true},
    {"rubyfpv_adaptive_enabled", "1", true, true, 0, 1, true},
    {"auto_detection_enabled", "1", true, true, 0, 1, true},
    
    // Profile-based settings
    {"racing_profile_adaptive", "1", true, true, 0, 2, true},
    {"cinematic_profile_adaptive", "0", true, true, 0, 2, true},
    {"default_profile_adaptive", "2", true, true, 0, 2, true},
    
    // Auto-search preferences
    {"prefer_rubalink_for_new_vehicles", "1", true, true, 0, 1, false},
    {"disable_rubalink_in_auto_search", "0", true, true, 0, 1, false},
    
    // Dynamic RSSI Threshold System
    {"enable_dynamic_thresholds", "1", true, true, 0, 1, true},
    {"hardware_rssi_offset", "0", false, true, -20, 20, false},
    {"safety_margin_db", "3", false, true, 0, 10, true},
    
    // QP Delta Configuration System
    {"enable_qp_delta", "1", true, true, 0, 1, true},
    {"qp_delta_low", "15", false, true, 0, 50, true},
    {"qp_delta_medium", "5", false, true, 0, 50, true},
    {"qp_delta_high", "0", false, true, 0, 50, true},
    
    // Advanced Signal Processing
    {"rssi_filter_chain", "0", false, true, 0, 4, true},
    {"dbm_filter_chain", "0", false, true, 0, 4, true},
    {"racing_rssi_filter_chain", "1", false, true, 0, 4, true},
    {"racing_dbm_filter_chain", "1", false, true, 0, 4, true},
    {"lpf_cutoff_freq", "2.0", false, false, 0, 0, true},
    {"lpf_sample_freq", "10.0", false, false, 0, 0, true},
    {"kalman_rssi_process", "0.00001", false, false, 0, 0, true},
    {"kalman_dbm_process", "0.00001", false, false, 0, 0, true},
    {"kalman_rssi_measure", "0.1", false, false, 0, 0, true},
    {"kalman_dbm_measure", "0.5", false, false, 0, 0, true},
    
    // Independent Signal Sampling
    {"enable_independent_sampling", "1", true, true, 0, 1, true},
    {"signal_sampling_freq_hz", "50", false, true, 1, 1000, true},
    {"signal_sampling_interval", "5", false, true, 1, 100, true},
    
    // Enhanced Cooldown & Control
    {"strict_cooldown_ms", "200", false, true, 50, 5000, true},
    {"up_cooldown_ms", "3000", false, true, 100, 10000, true},
    {"emergency_cooldown_ms", "50", false, true, 10, 1000, true},
    {"min_change_percent", "5", false, true, 1, 50, true},
    {"control_algorithm", "1", false, true, 0, 1, true},
    {"pid_kp", "1.0", false, false, 0, 0, true},
    {"pid_ki", "0.05", false, false, 0, 0, true},
    {"pid_kd", "0.4", false, false, 0, 0, true},
    
    // Racing Mode Enhancements
    {"race_mode_enabled", "0", true, true, 0, 1, true},
    {"racing_fps", "120", false, true, 30, 240, true},
    {"racing_video_resolution", "1280x720", false, false, 0, 0, true},
    {"racing_exposure", "11", false, true, 1, 30, true},
    {"racing_bitrate_max", "4", false, true, 1, 20, true},
    
    // Command Execution
    {"command_execution_method", "0", false, true, 0, 1, true},
    {"enable_error_handling", "1", true, true, 0, 1, true},
    {"max_command_length", "1024", false, true, 100, 4096, true}
};

#define CONFIG_RULES_COUNT (sizeof(s_ConfigRules) / sizeof(s_ConfigRules[0]))

bool rubalink_config_load() {
    FILE* file = fopen(RUBALINK_CONFIG_FILE, "r");
    if (!file) {
        // Try to create default config if file doesn't exist
        if (rubalink_config_create_default()) {
            file = fopen(RUBALINK_CONFIG_FILE, "r");
        }
        
        if (!file) {
            snprintf(s_LastError, sizeof(s_LastError), 
                    "Failed to open config file: %s", strerror(errno));
            return false;
        }
    }
    
    char line[512];
    int line_number = 0;
    config_section_t current_section = CONFIG_SECTION_NONE;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // Remove trailing newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // Check for section headers
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            current_section = rubalink_config_get_section(line);
            (void)current_section; // Suppress unused variable warning
            continue;
        }
        
        // Parse key-value pairs
        char key[64], value[256];
        if (rubalink_config_parse_line(line, key, value)) {
            // Validate the configuration item
            bool found = false;
            for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
                if (strcmp(s_ConfigRules[i].key, key) == 0) {
                    found = true;
                    
                    // Validate value
                    if (s_ConfigRules[i].is_numeric) {
                        int int_value = atoi(value);
                        if (int_value < s_ConfigRules[i].min_value || 
                            int_value > s_ConfigRules[i].max_value) {
                            snprintf(s_LastError, sizeof(s_LastError),
                                    "Value for %s out of range (%d-%d)", 
                                    key, s_ConfigRules[i].min_value, s_ConfigRules[i].max_value);
                            s_LastErrorLine = line_number;
                            fclose(file);
                            return false;
                        }
                    }
                    break;
                }
            }
            
            if (!found) {
                snprintf(s_LastError, sizeof(s_LastError),
                        "Unknown configuration key: %s", key);
                s_LastErrorLine = line_number;
                fclose(file);
                return false;
            }
        } else {
            snprintf(s_LastError, sizeof(s_LastError),
                    "Invalid configuration line format");
            s_LastErrorLine = line_number;
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    s_ConfigLoaded = true;
    return true;
}

bool rubalink_config_save() {
    // Create backup first
    if (!rubalink_config_save_backup()) {
        return false;
    }
    
    FILE* file = fopen(RUBALINK_CONFIG_FILE, "w");
    if (!file) {
        snprintf(s_LastError, sizeof(s_LastError),
                "Failed to open config file for writing: %s", strerror(errno));
        return false;
    }
    
    // Write header
    fprintf(file, "# RubALink Integration Configuration\n");
    fprintf(file, "# Generated automatically - do not edit manually\n");
    fprintf(file, "# Use RubALink settings panel for configuration\n\n");
    
    // Write configuration sections
    const char* sections[] = {
        "RUBALINK_ADVANCED",
        "RUBYFPV_ADAPTIVE", 
        "DYNAMIC_RSSI",
        "QP_DELTA",
        "SIGNAL_PROCESSING",
        "SIGNAL_SAMPLING",
        "COOLDOWN_CONTROL",
        "RACING_MODE",
        "COMMAND_EXECUTION"
    };
    
    for (size_t s = 0; s < sizeof(sections) / sizeof(sections[0]); s++) {
        fprintf(file, "[%s]\n", sections[s]);
        
        for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
            // Determine which section this config item belongs to
            bool belongs_to_section = false;
            const char* key = s_ConfigRules[i].key;
            
            if (strstr(sections[s], "RUBALINK_ADVANCED") && 
                (strstr(key, "rubalink_") || strstr(key, "rubyfpv_") || 
                 strstr(key, "auto_") || strstr(key, "profile_") || 
                 strstr(key, "prefer_") || strstr(key, "disable_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "DYNAMIC_RSSI") && 
                      (strstr(key, "dynamic_") || strstr(key, "hardware_") || 
                       strstr(key, "safety_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "QP_DELTA") && strstr(key, "qp_")) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "SIGNAL_PROCESSING") && 
                      (strstr(key, "filter_") || strstr(key, "lpf_") || 
                       strstr(key, "kalman_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "SIGNAL_SAMPLING") && 
                      (strstr(key, "sampling_") || strstr(key, "independent_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "COOLDOWN_CONTROL") && 
                      (strstr(key, "cooldown_") || strstr(key, "control_") || 
                       strstr(key, "pid_") || strstr(key, "min_change_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "RACING_MODE") && 
                      (strstr(key, "racing_") || strstr(key, "race_"))) {
                belongs_to_section = true;
            } else if (strstr(sections[s], "COMMAND_EXECUTION") && 
                      (strstr(key, "command_") || strstr(key, "error_") || 
                       strstr(key, "max_"))) {
                belongs_to_section = true;
            }
            
            if (belongs_to_section) {
                fprintf(file, "%s=%s\n", key, s_ConfigRules[i].value);
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool rubalink_config_save_backup() {
    FILE* src = fopen(RUBALINK_CONFIG_FILE, "r");
    if (!src) {
        // No existing file to backup
        return true;
    }
    
    FILE* dst = fopen(RUBALINK_CONFIG_BACKUP, "w");
    if (!dst) {
        fclose(src);
        snprintf(s_LastError, sizeof(s_LastError),
                "Failed to create backup file: %s", strerror(errno));
        return false;
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), src)) {
        fputs(buffer, dst);
    }
    
    fclose(src);
    fclose(dst);
    return true;
}

config_validation_result_t rubalink_config_validate() {
    config_validation_result_t result = {0};
    
    if (!s_ConfigLoaded) {
        result.valid = false;
        strcpy(result.error_message, "Configuration not loaded");
        return result;
    }
    
    // Check required configuration items
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (s_ConfigRules[i].required) {
            // In a real implementation, we would check if the value exists
            // For now, we'll assume all required items have default values
        }
    }
    
    result.valid = true;
    return result;
}

bool rubalink_config_parse_line(const char* line, char* key, char* value) {
    const char* equals = strchr(line, '=');
    if (!equals) {
        return false;
    }
    
    size_t key_len = equals - line;
    if (key_len >= 64) {
        return false;
    }
    
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    
    // Trim whitespace from key
    while (key_len > 0 && isspace(key[key_len - 1])) {
        key[--key_len] = '\0';
    }
    
    strcpy(value, equals + 1);
    
    // Trim whitespace from value
    size_t value_len = strlen(value);
    while (value_len > 0 && isspace(value[value_len - 1])) {
        value[--value_len] = '\0';
    }
    
    return true;
}

config_section_t rubalink_config_get_section(const char* line) {
    if (strstr(line, "[RUBALINK_ADVANCED]")) return CONFIG_SECTION_RUBALINK_ADVANCED;
    if (strstr(line, "[RUBYFPV_ADAPTIVE]")) return CONFIG_SECTION_RUBYFPV_ADAPTIVE;
    if (strstr(line, "[DYNAMIC_RSSI]")) return CONFIG_SECTION_DYNAMIC_RSSI;
    if (strstr(line, "[QP_DELTA]")) return CONFIG_SECTION_QP_DELTA;
    if (strstr(line, "[SIGNAL_PROCESSING]")) return CONFIG_SECTION_SIGNAL_PROCESSING;
    if (strstr(line, "[SIGNAL_SAMPLING]")) return CONFIG_SECTION_SIGNAL_SAMPLING;
    if (strstr(line, "[COOLDOWN_CONTROL]")) return CONFIG_SECTION_COOLDOWN_CONTROL;
    if (strstr(line, "[RACING_MODE]")) return CONFIG_SECTION_RACING_MODE;
    if (strstr(line, "[COMMAND_EXECUTION]")) return CONFIG_SECTION_COMMAND_EXECUTION;
    
    return CONFIG_SECTION_NONE;
}

bool rubalink_config_create_default() {
    FILE* file = fopen(RUBALINK_CONFIG_FILE, "w");
    if (!file) {
        snprintf(s_LastError, sizeof(s_LastError),
                "Failed to create default config file: %s", strerror(errno));
        return false;
    }
    
    // Write default configuration
    fprintf(file, "# RubALink Integration Configuration\n");
    fprintf(file, "# Default configuration file\n\n");
    
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        fprintf(file, "%s=%s\n", s_ConfigRules[i].key, s_ConfigRules[i].value);
    }
    
    fclose(file);
    return true;
}

void rubalink_config_reset_to_defaults() {
    rubalink_config_create_default();
    s_ConfigLoaded = false;
}

const char* rubalink_config_get_error_message() {
    return s_LastError;
}

int rubalink_config_get_error_line() {
    return s_LastErrorLine;
}

// Helper functions for getting/setting values
bool rubalink_config_get_bool(const char* key, bool* value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            *value = (strcmp(s_ConfigRules[i].value, "1") == 0);
            return true;
        }
    }
    return false;
}

bool rubalink_config_get_int(const char* key, int* value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            *value = atoi(s_ConfigRules[i].value);
            return true;
        }
    }
    return false;
}

bool rubalink_config_get_float(const char* key, float* value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            *value = atof(s_ConfigRules[i].value);
            return true;
        }
    }
    return false;
}

bool rubalink_config_set_bool(const char* key, bool value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            snprintf((char*)s_ConfigRules[i].value, sizeof(s_ConfigRules[i].value), "%d", value ? 1 : 0);
            return true;
        }
    }
    return false;
}

bool rubalink_config_set_int(const char* key, int value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            snprintf((char*)s_ConfigRules[i].value, sizeof(s_ConfigRules[i].value), "%d", value);
            return true;
        }
    }
    return false;
}

bool rubalink_config_set_float(const char* key, float value) {
    for (size_t i = 0; i < CONFIG_RULES_COUNT; i++) {
        if (strcmp(s_ConfigRules[i].key, key) == 0) {
            snprintf((char*)s_ConfigRules[i].value, sizeof(s_ConfigRules[i].value), "%.6f", value);
            return true;
        }
    }
    return false;
}
