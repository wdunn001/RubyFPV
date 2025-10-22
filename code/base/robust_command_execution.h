#pragma once

#include "../base/base.h"

// Command execution configuration
struct CommandExecutionConfig {
    int command_execution_method;  // 0=popen, 1=system
    bool enable_error_handling;    // Enable/disable error reporting
    int max_command_length;        // Maximum command length for security
};

// Function declarations
void robust_command_init();
int execute_command(const char *command);
int execute_command_silent(const char *command);
void robust_command_save_config();
void robust_command_load_config();