#include "rubalink_radio_interface.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/shared_mem.h"
#include "../common/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Global radio status
static rubalink_radio_status_t s_RadioStatus = {0};
static shared_mem_radio_stats* s_pSMRadioStats = NULL;

// Initialize RubALink radio interface integration
void rubalink_radio_interface_init() {
    log_line("[RubALink] Initializing radio interface integration...");
    
    memset(&s_RadioStatus, 0, sizeof(s_RadioStatus));
    
    // Get shared memory for radio stats
    s_pSMRadioStats = shared_mem_radio_stats_open_for_read();
    if (!s_pSMRadioStats) {
        log_softerror_and_alarm("[RubALink] Failed to open shared memory for radio stats");
        return;
    }
    
    // Initialize interface list from hardware
    s_RadioStatus.radio_interfaces_count = hardware_get_radio_interfaces_count();
    
    for (int i = 0; i < s_RadioStatus.radio_interfaces_count && i < MAX_RADIO_INTERFACES; i++) {
        rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[i];
        iface->interface_index = i;
        iface->pRadioInfo = hardware_get_radio_info(i);
        iface->last_rssi = -999;
        iface->last_dbm = -999;
        iface->last_dbm_noise = -999;
        iface->last_snr = -999;
        iface->current_mcs_rate = 0;
        iface->rate_control_enabled = true;
        iface->last_update_time = 0;
        
        if (iface->pRadioInfo) {
            log_line("[RubALink] Found radio interface %d: %s (%s), type: %d, driver: %d", 
                    i, iface->pRadioInfo->szName, iface->pRadioInfo->szUSBPort,
                    iface->pRadioInfo->iRadioType, iface->pRadioInfo->iRadioDriver);
        }
    }
    
    // Select primary interface (first WiFi interface)
    s_RadioStatus.primary_interface_index = -1;
    for (int i = 0; i < s_RadioStatus.radio_interfaces_count; i++) {
        if (rubalink_radio_is_wifi_interface(i)) {
            s_RadioStatus.primary_interface_index = i;
            break;
        }
    }
    
    log_line("[RubALink] Radio interface integration initialized with %d interfaces", 
            s_RadioStatus.radio_interfaces_count);
}

// Cleanup
void rubalink_radio_interface_cleanup() {
    log_line("[RubALink] Cleaning up radio interface integration...");
    
    if (s_pSMRadioStats) {
        shared_mem_radio_stats_close(s_pSMRadioStats);
        s_pSMRadioStats = NULL;
    }
    
    memset(&s_RadioStatus, 0, sizeof(s_RadioStatus));
}

// Update radio interface data from shared memory
void rubalink_radio_interface_update() {
    if (!s_pSMRadioStats) {
        return;
    }
    
    rubalink_radio_update_signal_stats();
    
    // Calculate VLQ for primary interface
    if (s_RadioStatus.primary_interface_index >= 0) {
        s_RadioStatus.primary_interface_vlq = rubalink_radio_calculate_vlq(s_RadioStatus.primary_interface_index);
    }
}

// Update signal stats from shared memory
bool rubalink_radio_update_signal_stats() {
    if (!s_pSMRadioStats) {
        return false;
    }
    
    unsigned long current_time = time(NULL);
    
    // Update each interface from shared memory
    for (int i = 0; i < s_RadioStatus.radio_interfaces_count && i < MAX_RADIO_INTERFACES; i++) {
        rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[i];
        
        if (i < s_pSMRadioStats->countLocalRadioInterfaces) {
            shared_mem_radio_stats_radio_interface* pSMInterface = &s_pSMRadioStats->radio_interfaces[i];
            
            // Update signal values from shared memory
            iface->last_dbm = pSMInterface->signalInfo.signalInfoAll.iDbmLast;
            iface->last_dbm_noise = pSMInterface->signalInfo.signalInfoAll.iDbmNoiseLast;
            iface->last_rssi = (iface->last_dbm > -200) ? (iface->last_dbm + 200) : 0;  // Convert from dBm to RSSI approximation
            
            // Calculate SNR
            if (iface->last_dbm > -200 && iface->last_dbm_noise > -200) {
                iface->last_snr = iface->last_dbm - iface->last_dbm_noise;
            } else {
                iface->last_snr = -999;
            }
            
            // Get current data rate (MCS)
            if (pSMInterface->lastRecvDataRate < 0) {
                // Negative value indicates MCS rate
                iface->current_mcs_rate = -pSMInterface->lastRecvDataRate - 1;
            }
            
            iface->last_update_time = current_time;
        }
    }
    
    return true;
}

// Calculate Video Link Quality (VLQ) for an interface
float rubalink_radio_calculate_vlq(int interface_index) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return 0.0f;
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    
    // Use the same VLQ calculation as AP-ALINK-FPV
    int dbm_Max = -50;
    int dbm_Min = -90;
    
    // Adjust dbm_Min based on RSSI (if available)
    if (iface->last_rssi > 55) {
        dbm_Min = -70;
    } else if (iface->last_rssi >= 40) {
        dbm_Min = -55;
    } else {
        dbm_Min = -53;
    }
    
    // Ensure dbm_Max is always greater than dbm_Min to prevent divide by zero
    if (dbm_Max <= dbm_Min) {
        dbm_Max = dbm_Min + 1;
    }
    
    float vlq;
    if (dbm_Max == dbm_Min) {
        // Fallback: if somehow they're still equal, use RSSI-based calculation
        vlq = (iface->last_rssi > 0) ? ((float)iface->last_rssi / 100.0f) * 100.0f : 0.0f;
    } else {
        // Additional safety check for invalid dBm values
        if (iface->last_dbm < -100 || iface->last_dbm > 0) {
            // Invalid dBm reading, use RSSI fallback
            vlq = (iface->last_rssi > 0) ? ((float)iface->last_rssi / 100.0f) * 100.0f : 0.0f;
        } else {
            vlq = ((float)(iface->last_dbm - dbm_Min) / (float)(dbm_Max - dbm_Min)) * 100.0f;
        }
    }
    
    // Clamp VLQ to valid range (0-100%)
    if (vlq < 0.0f) vlq = 0.0f;
    if (vlq > 100.0f) vlq = 100.0f;
    
    return vlq;
}

// Set MCS rate for WiFi interface
bool rubalink_radio_set_mcs_rate(int interface_index, int mcs_rate) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return false;
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    if (!iface->pRadioInfo || !rubalink_radio_is_wifi_interface(interface_index)) {
        return false;
    }
    
    // Check if this is a supported RTL88x2eu or RTL88xxau card
    if (iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU ||
        iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU ||
        iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU ||
        iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812AU) {
        
        // Construct the proc path based on driver type
        char proc_path[256];
        if (iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU) {
            snprintf(proc_path, sizeof(proc_path), "/proc/net/rtl88x2eu/%s/rate_ctl", 
                    iface->pRadioInfo->szName);
        } else {
            snprintf(proc_path, sizeof(proc_path), "/proc/net/rtl88xxau/%s/rate_ctl", 
                    iface->pRadioInfo->szName);
        }
        
        // Write MCS rate to proc file
        FILE* file = fopen(proc_path, "w");
        if (file) {
            fprintf(file, "0x%02x", mcs_rate);
            fclose(file);
            
            iface->current_mcs_rate = mcs_rate;
            log_line("[RubALink] Set MCS rate to %d (0x%02x) for interface %s", 
                    mcs_rate, mcs_rate, iface->pRadioInfo->szName);
            return true;
        } else {
            log_line("[RubALink] Failed to open %s for MCS rate control", proc_path);
        }
    }
    
    return false;
}

// Enable/disable rate control
bool rubalink_radio_set_rate_control(int interface_index, bool enable) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return false;
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    iface->rate_control_enabled = enable;
    
    log_line("[RubALink] Rate control %s for interface %d", 
            enable ? "enabled" : "disabled", interface_index);
    
    return true;
}

// Get current MCS rate
int rubalink_radio_get_current_mcs_rate(int interface_index) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return -1;
    }
    
    return s_RadioStatus.interfaces[interface_index].current_mcs_rate;
}

// Get interfaces count
int rubalink_radio_get_interfaces_count() {
    return s_RadioStatus.radio_interfaces_count;
}

// Get interface by index
rubalink_radio_interface_t* rubalink_radio_get_interface(int index) {
    if (index < 0 || index >= s_RadioStatus.radio_interfaces_count) {
        return NULL;
    }
    
    return &s_RadioStatus.interfaces[index];
}

// Get radio status
rubalink_radio_status_t* rubalink_radio_get_status() {
    return &s_RadioStatus;
}

// Check if interface is WiFi
bool rubalink_radio_is_wifi_interface(int interface_index) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return false;
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    if (!iface->pRadioInfo) {
        return false;
    }
    
    // Check if it's a WiFi radio type
    return (iface->pRadioInfo->iRadioType != RADIO_TYPE_SIK && 
            iface->pRadioInfo->iRadioType != RADIO_TYPE_SERIAL);
}

// Check if interface supports rate control
bool rubalink_radio_supports_rate_control(int interface_index) {
    if (!rubalink_radio_is_wifi_interface(interface_index)) {
        return false;
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    if (!iface->pRadioInfo) {
        return false;
    }
    
    // Check if it's a supported RTL88x2eu or RTL88xxau card
    return (iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU ||
            iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU ||
            iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU ||
            iface->pRadioInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812AU);
}

// Get interface name
const char* rubalink_radio_get_interface_name(int interface_index) {
    if (interface_index < 0 || interface_index >= s_RadioStatus.radio_interfaces_count) {
        return "Unknown";
    }
    
    rubalink_radio_interface_t* iface = &s_RadioStatus.interfaces[interface_index];
    if (iface->pRadioInfo) {
        return iface->pRadioInfo->szName;
    }
    
    return "Unknown";
}
