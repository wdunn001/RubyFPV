#include <stdio.h>
#include <stdlib.h>
#include "code/r_vehicle/adaptive_video_rubalink.h"

int main() {
    printf("Testing RubALink Integration with RubyFPV Adaptive Video\n");
    printf("========================================================\n\n");
    
    // Initialize RubALink enhancements
    printf("1. Initializing RubALink enhancements...\n");
    adaptive_video_rubalink_init();
    
    // Test signal filtering
    printf("\n2. Testing Signal Filtering:\n");
    float raw_rssi = 65.0f;
    float filtered_rssi = adaptive_video_apply_kalman_filter(raw_rssi, true);
    printf("   Raw RSSI: %.1f, Kalman Filtered: %.1f\n", raw_rssi, filtered_rssi);
    
    // Test dynamic RSSI thresholds
    printf("\n3. Testing Dynamic RSSI Thresholds:\n");
    for (int mcs = 0; mcs <= 8; mcs += 2) {
        int threshold = adaptive_video_get_dynamic_rssi_threshold(mcs);
        printf("   MCS %d: RSSI threshold = %d dBm\n", mcs, threshold);
    }
    
    // Test QP Delta
    printf("\n4. Testing QP Delta for Different Bitrates:\n");
    int bitrates[] = {1, 2, 4, 6, 8, 10};
    for (int i = 0; i < 6; i++) {
        int qp_delta = adaptive_video_get_qp_delta_for_bitrate(bitrates[i]);
        printf("   %d Mbps: QP Delta = %d\n", bitrates[i], qp_delta);
    }
    
    // Test cooldown
    printf("\n5. Testing Enhanced Cooldown:\n");
    int current_bitrate = 4000; // 4 Mbps
    int new_bitrate = 6000; // 6 Mbps
    bool allowed = adaptive_video_check_enhanced_cooldown(new_bitrate, current_bitrate);
    printf("   Change from %d to %d kbps: %s\n", 
           current_bitrate, new_bitrate, allowed ? "ALLOWED" : "BLOCKED");
    
    // Test racing mode
    printf("\n6. Testing Racing Mode:\n");
    adaptive_video_enable_racing_mode(true);
    printf("   Racing mode active: %s\n", 
           adaptive_video_is_racing_mode_active() ? "YES" : "NO");
    
    float vlq_values[] = {90.0f, 70.0f, 50.0f, 20.0f};
    const char* vlq_labels[] = {"Excellent", "Good", "Fair", "Poor"};
    
    for (int i = 0; i < 4; i++) {
        int racing_bitrate = adaptive_video_calculate_racing_bitrate(current_bitrate, vlq_values[i]);
        printf("   VLQ %.0f%% (%s): Target bitrate = %d kbps\n", 
               vlq_values[i], vlq_labels[i], racing_bitrate);
    }
    
    // Test MCS optimization
    printf("\n7. Testing MCS Rate Optimization:\n");
    float rssi_values[] = {80.0f, 65.0f, 50.0f, 35.0f};
    float packet_loss[] = {0.5f, 3.0f, 7.0f, 15.0f};
    
    for (int i = 0; i < 4; i++) {
        int optimal_mcs = adaptive_video_get_optimal_mcs_for_conditions(rssi_values[i], packet_loss[i]);
        printf("   RSSI %.0f, Loss %.1f%%: Optimal MCS = %d\n", 
               rssi_values[i], packet_loss[i], optimal_mcs);
    }
    
    // Cleanup
    printf("\n8. Cleaning up...\n");
    adaptive_video_rubalink_cleanup();
    
    printf("\nIntegration test completed successfully!\n");
    return 0;
}
