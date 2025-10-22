#pragma once

#include "base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Configuration file paths
#define RUBALINK_CONFIG_FILE "/home/radxa/RubyFPV/config/rubalink_integration.cfg"
#define RUBALINK_CONFIG_BACKUP "/home/radxa/RubyFPV/config/rubalink_integration.cfg.bak"

// Configuration sections
typedef enum {
    CONFIG_SECTION_NONE = 0,
    CONFIG_SECTION_RUBALINK_ADVANCED,
    CONFIG_SECTION_RUBYFPV_ADAPTIVE,
    CONFIG_SECTION_DYNAMIC_RSSI,
    CONFIG_SECTION_QP_DELTA,
    CONFIG_SECTION_SIGNAL_PROCESSING,
    CONFIG_SECTION_SIGNAL_SAMPLING,
    CONFIG_SECTION_COOLDOWN_CONTROL,
    CONFIG_SECTION_RACING_MODE,
    CONFIG_SECTION_COMMAND_EXECUTION
} config_section_t;

// Configuration validation result
typedef struct {
    bool valid;
    char error_message[256];
    int error_line;
} config_validation_result_t;

// Configuration item
typedef struct {
    char key[64];
    char value[256];
    bool is_boolean;
    bool is_numeric;
    int min_value;
    int max_value;
    bool required;
} config_item_t;

// Function declarations
bool rubalink_config_load();
bool rubalink_config_save();
bool rubalink_config_save_backup();
config_validation_result_t rubalink_config_validate();
bool rubalink_config_parse_line(const char* line, char* key, char* value);
config_section_t rubalink_config_get_section(const char* line);
bool rubalink_config_set_value(const char* key, const char* value);
bool rubalink_config_get_value(const char* key, char* value, size_t value_size);
bool rubalink_config_get_bool(const char* key, bool* value);
bool rubalink_config_get_int(const char* key, int* value);
bool rubalink_config_get_float(const char* key, float* value);
bool rubalink_config_set_bool(const char* key, bool value);
bool rubalink_config_set_int(const char* key, int value);
bool rubalink_config_set_float(const char* key, float value);
void rubalink_config_reset_to_defaults();
bool rubalink_config_create_default();
const char* rubalink_config_get_error_message();
int rubalink_config_get_error_line();
