#include <bit.h>

#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

static int bit_do_parse_field1_2(struct bit* bit) {
	static const uint8_t header[] = {0x00,0x09,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x00,0x00,0x01};

	if (bit->size < sizeof(header) || memcmp(bit->data, header, sizeof(header)) != 0) {
		bit->error_str = "Wrong bit header";
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

	bit->data += len;
	bit->size -= len;

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

const char* bit_get_error_string(struct bit* bit) {
	return bit->error_str;
}
