#ifndef _OV_H
#define _OV_H

#include <stddef.h>
#include <stdint.h>

struct ov_device;

struct ov_packet {
	uint16_t flags;
	uint16_t size;
	uint32_t timestamp;
	uint8_t  data[];
};

typedef void (*ov_packet_decoder_callback)(struct ov_packet*, void*);

int  ov_init(struct ov_device* ov);
int  ov_open(struct ov_device* ov);
void ov_destroy(struct ov_device* ov);

int ov_capture_start(struct ov_device* ov, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data);
int ov_capture_dispatch(struct ov_device* ov, int count);
int ov_capture_stop(struct ov_device* ov);

const char* ov_get_error_string(struct ov_device* ov);

#endif // _OV_H
