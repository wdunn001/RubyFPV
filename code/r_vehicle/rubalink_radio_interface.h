#pragma once

#include "../base/base.h"
#include "../base/hardware_radio.h"
#include "../base/shared_mem_radio.h"
#include <stdint.h>
#include <stdbool.h>

// RubALink Radio Interface Integration
// Uses RubyFPV's existing radio interface system

#define MAX_WIFI_CARDS 4  // For compatibility

typedef struct {
    int interface_index;
    radio_hw_info_t* pRadioInfo;
    int last_rssi;
    int last_dbm;
    int last_dbm_noise;
    int last_snr;
    int current_mcs_rate;
    bool rate_control_enabled;
    unsigned long last_update_time;
} rubalink_radio_interface_t;

typedef struct {
    int radio_interfaces_count;
    rubalink_radio_interface_t interfaces[MAX_RADIO_INTERFACES];
    float primary_interface_vlq;  // Video Link Quality 0-100%
    int primary_interface_index;
} rubalink_radio_status_t;

// Function declarations
void rubalink_radio_interface_init();
void rubalink_radio_interface_cleanup();
void rubalink_radio_interface_update();

// Radio interface management
int rubalink_radio_get_interfaces_count();
rubalink_radio_interface_t* rubalink_radio_get_interface(int index);
rubalink_radio_status_t* rubalink_radio_get_status();

// Signal reading from shared memory
bool rubalink_radio_update_signal_stats();
float rubalink_radio_calculate_vlq(int interface_index);

// MCS rate control for WiFi interfaces
bool rubalink_radio_set_mcs_rate(int interface_index, int mcs_rate);
bool rubalink_radio_set_rate_control(int interface_index, bool enable);
int rubalink_radio_get_current_mcs_rate(int interface_index);

// Integration with RubALink monitoring
void rubalink_radio_update_monitoring_data();

// Utility functions
bool rubalink_radio_is_wifi_interface(int interface_index);
bool rubalink_radio_supports_rate_control(int interface_index);
const char* rubalink_radio_get_interface_name(int interface_index);
