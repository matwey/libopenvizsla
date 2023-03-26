/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _DECODER_H
#define _DECODER_H

#include <memory.h>
#include <stdint.h>

#include <openvizsla.h>

struct decoder_ops {
	void (*packet) (void*, struct ov_packet*);
	void (*bus_frame) (void*, uint16_t, uint8_t);
};

struct packet_decoder {
	struct ov_packet* packet;

	struct decoder_ops ops;
	void* user_data;

	enum packet_decoder_state {
		NEED_PACKET_MAGIC,
		NEED_PACKET_FLAGS_LO,
		NEED_PACKET_FLAGS_HI,
		NEED_PACKET_LENGTH_LO,
		NEED_PACKET_LENGTH_HI,
		NEED_PACKET_TIMESTAMP_LO,
		NEED_PACKET_TIMESTAMP_ME,
		NEED_PACKET_TIMESTAMP_HI,
		NEED_PACKET_DATA
	} state;

	size_t buf_actual_length;
	size_t buf_length;

	char* error_str;
};

int packet_decoder_init(struct packet_decoder* pd, struct ov_packet* p, size_t size, const struct decoder_ops* ops, void* user_data);
int packet_decoder_proc(struct packet_decoder* pd, uint8_t* buf, size_t size);

struct frame_decoder {
	struct packet_decoder pd;

	enum frame_decoder_state {
		NEED_FRAME_MAGIC,
		NEED_SDRAM_FRAME_LENGTH,
		NEED_SDRAM_FRAME_DATA,
		NEED_BUS_FRAME_ADDR_HI,
		NEED_BUS_FRAME_ADDR_LO,
		NEED_BUS_FRAME_VALUE,
		NEED_BUS_FRAME_CHECKSUM
	} state;

	union {
	struct {
		uint16_t required_length;
	} sdram;
	struct {
		uint16_t addr;
		uint8_t value;
		uint8_t checksum;
	} bus;
	};
};

int frame_decoder_init(struct frame_decoder* fd, struct ov_packet* p, size_t size, const struct decoder_ops* ops, void* user_data);
int frame_decoder_proc(struct frame_decoder* fd, uint8_t* buf, size_t size);

#endif // _DECODER_H
