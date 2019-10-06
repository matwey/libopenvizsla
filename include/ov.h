/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _OV_H
#define _OV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#	define ov_to_host_16(x) (x)
#	define ov_to_host_24(x) (x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#	define ov_to_host_16(x) (__builtin_bswap16(x))
#	define ov_to_host_24(x) (__builtin_bswap32(x) >> 8)
#else
#	error "Unknown platform byte order"
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
