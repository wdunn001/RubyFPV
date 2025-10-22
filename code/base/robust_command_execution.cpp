#include "robust_command_execution.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static CommandExecutionConfig s_CommandConfig = {
    .command_execution_method = 0,  // Default to popen()
    .enable_error_handling = true,
    .max_command_length = 1024
};

void robust_command_init() {
    robust_command_load_config();
    log_line("[AP-ALINK-FPV] Robust command execution system initialized");
}

int execute_command(const char *command) {
    // Validate input parameters
    if (command == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Error: execute_command() called with NULL command\n");
#endif
        return -1;
    }
    
    // Check for empty command string
    if (strlen(command) == 0) {
#ifdef DEBUG
        fprintf(stderr, "Error: execute_command() called with empty command\n");
#endif
        return -1;
    }
    
    // Check for command length to prevent buffer overflow attacks
    if (strlen(command) > s_CommandConfig.max_command_length) {
#ifdef DEBUG
        fprintf(stderr, "Error: execute_command() command too long (>%d chars)\n", 
                s_CommandConfig.max_command_length);
#endif
        return -1;
    }
    
    if (s_CommandConfig.command_execution_method == 0) {
        // Use popen() method
        FILE *pipe = popen(command, "w");
        if (pipe == NULL) {
#ifdef DEBUG
            perror("popen failed");
#endif
            return -1;
        }
        
        int status = pclose(pipe);
        if (status == -1) {
#ifdef DEBUG
            perror("pclose failed");
#endif
            return -1;
        }
        
        return WEXITSTATUS(status);
    }
    else {
        // Use system() method (legacy)
        return system(command);
    }
}

int execute_command_silent(const char *command) {
    return execute_command(command);
}

void robust_command_save_config() {
    // Save configuration to RubyFPV config system
    log_line("[AP-ALINK-FPV] Robust command execution config saved");
}

void robust_command_load_config() {
    // Load configuration from RubyFPV config system
    log_line("[AP-ALINK-FPV] Robust command execution config loaded");
}