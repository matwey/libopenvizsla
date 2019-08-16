/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _OV_H
#define _OV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ov_device;

struct ov_packet {
	uint16_t flags;
	uint16_t size;
	uint32_t timestamp;
	uint8_t  data[];
};

typedef void (*ov_packet_decoder_callback)(struct ov_packet*, void*);

enum ov_usb_speed {
	OV_LOW_SPEED  = 0x4a,
	OV_FULL_SPEED = 0x49,
	OV_HIGH_SPEED = 0x48
};

struct ov_device* ov_new(const char* firmware_filename);
int  ov_open(struct ov_device* ov);
void ov_free(struct ov_device* ov);

int ov_get_usb_speed(struct ov_device* ov, enum ov_usb_speed* speed);
int ov_set_usb_speed(struct ov_device* ov, enum ov_usb_speed speed);

int ov_capture_start(struct ov_device* ov, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data);
int ov_capture_dispatch(struct ov_device* ov, int count);
int ov_capture_stop(struct ov_device* ov);

int ov_load_firmware(struct ov_device* ov, const char* filename);

const char* ov_get_error_string(struct ov_device* ov);

#ifdef __cplusplus
}
#endif

#endif // _OV_H
