#include "rubalink_monitoring_display.h"
#include "../r_vehicle/rubalink_integration.h"
#include "../r_vehicle/rubalink_radio_interface.h"
#include "../r_vehicle/adaptive_video_rubalink.h"
#include <sys/time.h>

// Helper function to get current time in milliseconds
static unsigned long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static rubalink_monitoring_data_t s_MonitoringData = {0};
static bool s_MonitoringDisplayActive = false;

void rubalink_monitoring_display_init() {
    log_line("[RubALink] Monitoring display initialized");
    memset(&s_MonitoringData, 0, sizeof(s_MonitoringData));
}

void rubalink_monitoring_display_cleanup() {
    log_line("[RubALink] Monitoring display cleanup");
    s_MonitoringDisplayActive = false;
}

void rubalink_monitoring_display_show() {
    s_MonitoringDisplayActive = true;
    log_line("[RubALink] Monitoring display shown");
}

void rubalink_monitoring_display_hide() {
    s_MonitoringDisplayActive = false;
    log_line("[RubALink] Monitoring display hidden");
}

bool rubalink_monitoring_display_is_active() {
    return s_MonitoringDisplayActive;
}

void rubalink_monitoring_update_data() {
    if (!s_MonitoringDisplayActive) return;
    
    rubalink_monitoring_update_signal_processing_status();
    rubalink_monitoring_update_cooldown_status();
    rubalink_monitoring_update_racing_mode_status();
    rubalink_monitoring_update_performance_metrics();
}

void rubalink_monitoring_update_signal_processing_status() {
    // TODO: Get actual data from signal processing modules
    s_MonitoringData.advanced_signal_processing_active = true;
    s_MonitoringData.rssi_filter_chain_active = 0; // Kalman
    s_MonitoringData.dbm_filter_chain_active = 0; // Kalman
    s_MonitoringData.filtered_rssi = 75.5f;
    s_MonitoringData.filtered_dbm = -45.2f;
    s_MonitoringData.signal_sampling_rate = 50.0f;
    
    // Update radio interface data
    rubalink_radio_interface_update();
    
    rubalink_radio_status_t* radio_status = rubalink_radio_get_status();
    s_MonitoringData.radio_interfaces_count = radio_status->radio_interfaces_count;
    s_MonitoringData.primary_interface_index = radio_status->primary_interface_index;
    
    // Copy radio interface status
    for (int i = 0; i < s_MonitoringData.radio_interfaces_count && i < MAX_RADIO_INTERFACES; i++) {
        rubalink_radio_interface_t* iface = rubalink_radio_get_interface(i);
        if (iface) {
            s_MonitoringData.radio_interfaces[i] = *iface;
        }
    }
    
    // Get VLQ from radio status
    s_MonitoringData.radio_signal_quality = radio_status->primary_interface_vlq;
}

void rubalink_monitoring_update_cooldown_status() {
    // TODO: Get actual data from cooldown modules
    s_MonitoringData.enhanced_cooldown_active = true;
    s_MonitoringData.control_algorithm_active = 1; // Simple FIFO
    s_MonitoringData.last_bitrate_change_time = get_current_time_ms() - 1500;
    s_MonitoringData.current_bitrate = 4;
    s_MonitoringData.target_bitrate = 6;
}

void rubalink_monitoring_update_racing_mode_status() {
    // TODO: Get actual data from racing mode modules
    s_MonitoringData.racing_mode_active = false;
    s_MonitoringData.racing_fps = 120;
    strcpy(s_MonitoringData.racing_video_resolution, "1280x720");
    s_MonitoringData.racing_exposure = 11;
}

void rubalink_monitoring_update_performance_metrics() {
    // TODO: Calculate actual performance metrics
    s_MonitoringData.bitrate_changes_per_minute = 3;
    s_MonitoringData.average_signal_stability = 85.2f;
    s_MonitoringData.emergency_drops_count = 0;
    s_MonitoringData.uptime_ms = get_current_time_ms();
}

void rubalink_monitoring_render() {
    if (!s_MonitoringDisplayActive) return;
    
    rubalink_monitoring_render_status_overlay();
    rubalink_monitoring_render_signal_processing_info();
    rubalink_monitoring_render_cooldown_info();
    rubalink_monitoring_render_racing_mode_info();
    rubalink_monitoring_render_performance_info();
}

void rubalink_monitoring_render_status_overlay() {
    // TODO: Implement actual rendering
    log_line("[RubALink] Rendering status overlay");
}

void rubalink_monitoring_render_signal_processing_info() {
    // TODO: Implement actual rendering
    log_line("[RubALink] Rendering signal processing info");
}

void rubalink_monitoring_render_cooldown_info() {
    // TODO: Implement actual rendering
    log_line("[RubALink] Rendering cooldown info");
}

void rubalink_monitoring_render_racing_mode_info() {
    // TODO: Implement actual rendering
    log_line("[RubALink] Rendering racing mode info");
}

void rubalink_monitoring_render_performance_info() {
    // TODO: Implement actual rendering
    log_line("[RubALink] Rendering performance info");
}

rubalink_monitoring_data_t* rubalink_monitoring_get_data() {
    return &s_MonitoringData;
}

void rubalink_monitoring_set_data(const rubalink_monitoring_data_t* data) {
    s_MonitoringData = *data;
}

void rubalink_monitoring_reset_statistics() {
    s_MonitoringData.bitrate_changes_per_minute = 0;
    s_MonitoringData.average_signal_stability = 0.0f;
    s_MonitoringData.emergency_drops_count = 0;
    s_MonitoringData.uptime_ms = get_current_time_ms();
    log_line("[RubALink] Monitoring statistics reset");
}

void rubalink_monitoring_get_statistics(char* stats_buffer, int max_length) {
    if (!stats_buffer || max_length <= 0) return;
    
    snprintf(stats_buffer, max_length,
             "RubALink Monitoring: MCS=%d, QP=%d, RSSI=%.1f, dBm=%.1f, Bitrate=%d, Racing=%s",
             s_MonitoringData.current_mcs,
             s_MonitoringData.current_qp_delta,
             s_MonitoringData.filtered_rssi,
             s_MonitoringData.filtered_dbm,
             s_MonitoringData.current_bitrate,
             s_MonitoringData.racing_mode_active ? "Yes" : "No");
}
