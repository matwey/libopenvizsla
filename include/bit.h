/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _BIT_H
#define _BIT_H

#include <cha.h>
#include <chb.h>

#include <memory.h>
#include <stdint.h>

struct bit {
	const uint8_t* data;
	size_t size;

	const char* ncd_filename;
	const char* part_name;
	const char* date;
	const char* time;
	size_t bit_length;

	const char* error_str;
};

int bit_init(struct bit* bit, const void* data, size_t size);
int bit_load_firmware(struct bit* bit, struct cha* cha, struct chb* chb);

const char* bit_get_error_string(struct bit* bit);

#endif // _BIT_H
