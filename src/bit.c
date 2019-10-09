/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <bit.h>

#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

#define PORTB_DONE_BIT (1 << 2)

inline static uint8_t bitreverse8(uint8_t x) {
	static const uint8_t map[256] = {
#define R2(n) n, n + 2*64, n + 1*64, n + 3*64
#define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#define R6(n) R4(n), R4(n + 2*4), R4(n + 1*4), R4(n + 3*4)
	R6(0), R6(2), R6(1), R6(3)
#undef R6
#undef R4
#undef R2
	};

	return map[x];
}

static int bit_do_parse_field1_2(struct bit* bit) {
	static const uint8_t header[] = {0x00,0x09,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x00,0x00,0x01};

	if (bit->size < sizeof(header) || memcmp(bit->data, header, sizeof(header)) != 0) {
		bit->error_str = "Wrong bitstream header";
		return -1;
	}

	bit->data += sizeof(header);
	bit->size -= sizeof(header);

	return 0;
}

static int bit_do_parse_field_str(struct bit* bit, uint8_t key, const char** str) {
	uint16_t len;

	if (bit->size < sizeof(key) + sizeof(len)) {
		bit->error_str = "Too few bytes";
		return -1;
	}

	if (((uint8_t*)bit->data)[0] != key) {
		bit->error_str = "Incorrent field key";
		return -1;
	}

	bit->data += sizeof(key);
	bit->size -= sizeof(key);

	memcpy(&len, bit->data, sizeof(len));
	len = htons(len);

	bit->data += sizeof(len);
	bit->size -= sizeof(len);

	if (bit->size < len) {
		bit->error_str = "Too few bytes";
		return -1;
	}

	/* string is null-terminated */
	*str = bit->data;
	bit->data += len;
	bit->size -= len;

	return 0;
}

static int bit_do_parse_field2_3(struct bit* bit) {
	return bit_do_parse_field_str(bit, 'a', &bit->ncd_filename);
}

static int bit_do_parse_field4(struct bit* bit) {
	return bit_do_parse_field_str(bit, 'b', &bit->part_name);
}

static int bit_do_parse_field5(struct bit* bit) {
	return bit_do_parse_field_str(bit, 'c', &bit->date);
}

static int bit_do_parse_field6(struct bit* bit) {
	return bit_do_parse_field_str(bit, 'd', &bit->time);
}

static int bit_do_parse_field7(struct bit* bit) {
	uint32_t len;
	const uint8_t key = 'e';

	if (bit->size < sizeof(key) + sizeof(len)) {
		bit->error_str = "Too few bytes";
		return -1;
	}

	if (((uint8_t*)bit->data)[0] != key) {
		bit->error_str = "Incorrent field key";
		return -1;
	}

	bit->data += sizeof(key);
	bit->size -= sizeof(key);

	memcpy(&len, bit->data, sizeof(len));
	len = htonl(len);

	bit->bit_length = len;
	bit->data += sizeof(len);
	bit->size -= sizeof(len);

	if (bit->size < len) {
		bit->error_str = "Too few bytes";
		return -1;
	}

	return 0;
}

/*
 * http://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm
 */
static int bit_do_parse(struct bit* bit) {
	if (bit_do_parse_field1_2(bit) < 0)
		return -1;
	if (bit_do_parse_field2_3(bit) < 0)
		return -1;
	if (bit_do_parse_field4(bit) < 0)
		return -1;
	if (bit_do_parse_field5(bit) < 0)
		return -1;
	if (bit_do_parse_field6(bit) < 0)
		return -1;
	if (bit_do_parse_field7(bit) < 0)
		return -1;

	return 0;
}

int bit_init(struct bit* bit, const void* data, size_t size) {
	bit->data = data;
	bit->size = size;

	return bit_do_parse(bit);
}

int bit_load_firmware(struct bit* bit, struct cha* cha, struct chb* chb) {
	uint8_t buf[2][4 * 1024];
	size_t bidx = 0;
	const uint8_t* data = bit->data;
	size_t length = bit->size;
	struct ftdi_transfer_control* tc = NULL;
	int ret;
	uint8_t status;
	int try = 3;

	for (bidx = 0; length; bidx = (bidx + 1) % 2) {
		size_t i = 0;

		for (i = 0; i < sizeof(buf[bidx]) && length; i++, length--, data++) {
			buf[bidx][i] = bitreverse8(*data);
		}

		if (tc && ftdi_transfer_data_done(tc) < 0) {
			bit->error_str = ftdi_get_error_string(&cha->ftdi);
			return -1;
		}

		if (!(tc = ftdi_write_data_submit(&cha->ftdi, buf[bidx], i))) {
			bit->error_str = ftdi_get_error_string(&cha->ftdi);
			return -1;
		}
	}

	if (ftdi_transfer_data_done(tc) < 0) {
		bit->error_str = ftdi_get_error_string(&cha->ftdi);
		return -1;
	}

	uint8_t init_cycles[8];
	memset(init_cycles, 0, sizeof(init_cycles));

	for (try = 3;
		try && (ret = ftdi_write_data(&cha->ftdi, init_cycles, sizeof(init_cycles))) > 0
		&& (ret = chb_get_high(chb, &status)) == 0
		&& !(status & PORTB_DONE_BIT);
		--try);

	if (ret < 0)
		return -1;

	if (try == 0 && !(status & PORTB_DONE_BIT)) {
		bit->error_str = "done bit stuck low after programming";
		return -1;
	}

	return 0;
}

const char* bit_get_error_string(struct bit* bit) {
	return bit->error_str;
}
