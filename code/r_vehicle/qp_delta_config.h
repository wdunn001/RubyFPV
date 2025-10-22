#pragma once

#include "../base/base.h"

// QP Delta configuration structure
struct QPDeltaConfig {
    int qp_delta_low;      // QP delta for low bitrate (MCS 1-2)
    int qp_delta_medium;   // QP delta for medium bitrate (MCS 3-9)
    int qp_delta_high;     // QP delta for high bitrate (MCS 10+)
    bool enable_qp_delta;  // Enable/disable feature
};

// Function declarations
void qp_delta_init();
void qp_delta_save_config();
void qp_delta_load_config();
void apply_qp_delta_for_bitrate(int bitrate_mbps);
int get_qp_delta_for_bitrate(int bitrate_mbps);
void set_qp_delta_presets(const char* preset_name);