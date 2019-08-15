/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _CHA_H
#define _CHA_H

#include <decoder.h>
#include <ftdi.h>
#include <ov.h>
#include <reg.h>

#include <stdint.h>
#include <memory.h>

struct cha {
	struct ftdi_context ftdi;
	struct reg reg;
	const char* error_str;
};

struct cha_loop {
	struct cha* cha;

	ov_packet_decoder_callback callback;
	void* user_data;

	int count;
	int max_count;
	int break_loop;
	int complete;
	struct frame_decoder fd;
};

int cha_init(struct cha* cha, struct fwpkg* fwpkg);
int cha_open(struct cha* cha);
int cha_switch_config_mode(struct cha* cha);
int cha_switch_fifo_mode(struct cha* cha);
int cha_write_reg(struct cha* cha, uint16_t addr, uint8_t val);
int cha_read_reg(struct cha* cha, uint16_t addr, uint8_t* val);
int cha_write_reg32(struct cha* cha, uint16_t addr, uint32_t val);
int cha_read_reg32(struct cha* cha, uint16_t addr, uint32_t* val);
int cha_write_reg_by_name(struct cha* cha, enum reg_name name, uint8_t val);
int cha_read_reg_by_name(struct cha* cha, enum reg_name name, uint8_t* val);
int cha_write_reg32_by_name(struct cha* cha, enum reg_name name, uint32_t val);
int cha_read_reg32_by_name(struct cha* cha, enum reg_name name, uint32_t* val);
int cha_write_ulpi(struct cha* cha, uint8_t addr, uint8_t val);
int cha_read_ulpi(struct cha* cha, uint8_t addr, uint8_t* val);
int cha_get_usb_speed(struct cha* cha, enum ov_usb_speed* speed);
int cha_set_usb_speed(struct cha* cha, enum ov_usb_speed speed);
int cha_start_stream(struct cha* cha);
int cha_stop_stream(struct cha* cha);
int cha_loop_init(struct cha_loop* loop, struct cha* cha, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data);
int cha_loop_run(struct cha_loop* loop, int count);
int cha_set_reg(struct cha* cha, struct reg* reg);
void cha_destroy(struct cha* cha);

const char* cha_get_error_string(struct cha* cha);

#endif // _CHA_H
