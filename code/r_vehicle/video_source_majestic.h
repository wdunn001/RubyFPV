#pragma once
#include "../base/base.h"
#include "../base/models.h"

#define MAJESTIC_UDP_PORT 5600

void video_source_majestic_apply_all_parameters();
void video_source_majestic_start_program();
void video_source_majestic_stop_program();
u32 video_source_majestic_get_program_start_time();

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync);
u8* video_source_majestic_raw_read(int* piReadSize, bool bAsync);
int video_source_majestic_get_audio_data(u8* pOutputBuffer, int iMaxToRead);
void video_source_majestic_clear_audio_buffers();
void video_source_majestic_clear_input_buffers();

bool video_source_majestic_last_read_is_single_nal();
bool video_source_majestic_last_read_is_end_nal();
u32 video_source_majestic_get_last_nal_type();
bool video_source_majestic_periodic_health_checks();
