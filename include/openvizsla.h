/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _OPENVIZSLA_H
#define _OPENVIZSLA_H

#include <stddef.h>
#include <stdint.h>

#include <openvizsla_export.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ov_device;

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct
#ifdef __GNUC__
	__attribute__((packed))
#elif !defined(_MSC_VER)
	#error Please provide __attribute__((packed)) for your compiler
#endif
ov_packet {
	uint8_t  magic;
	uint16_t flags;
	uint16_t size;
	uint32_t timestamp : 24;
	uint8_t  data[];
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef void (*ov_packet_decoder_callback)(struct ov_packet*, void*);

enum ov_usb_speed {
	OV_LOW_SPEED  = 0x4a,
	OV_FULL_SPEED = 0x49,
	OV_HIGH_SPEED = 0x48
};

OPENVIZSLA_EXPORT struct ov_device* ov_new(const char* firmware_filename);
OPENVIZSLA_EXPORT int  ov_open(struct ov_device* ov);
OPENVIZSLA_EXPORT void ov_free(struct ov_device* ov);

OPENVIZSLA_EXPORT int ov_get_usb_speed(struct ov_device* ov, enum ov_usb_speed* speed);
OPENVIZSLA_EXPORT int ov_set_usb_speed(struct ov_device* ov, enum ov_usb_speed speed);

OPENVIZSLA_EXPORT int ov_capture_start(struct ov_device* ov, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data);
OPENVIZSLA_EXPORT int ov_capture_dispatch(struct ov_device* ov, int count);
OPENVIZSLA_EXPORT void ov_capture_breakloop(struct ov_device* ov);
OPENVIZSLA_EXPORT ov_packet_decoder_callback ov_capture_set_callback(struct ov_device* ov, ov_packet_decoder_callback callback, void* user_data);
OPENVIZSLA_EXPORT int ov_capture_stop(struct ov_device* ov);

OPENVIZSLA_EXPORT int ov_load_firmware(struct ov_device* ov, const char* filename);

OPENVIZSLA_EXPORT const char* ov_get_error_string(struct ov_device* ov);

#ifdef __cplusplus
}
#endif

#endif // _OPENVIZSLA_H
