#ifndef _BIT_H
#define _BIT_H

#include <memory.h>

struct bit {
	const void* data;
	size_t size;

	const char* ncd_filename;
	const char* part_name;
	const char* date;
	const char* time;
	size_t bit_length;

	const char* error_str;
};

int bit_init(struct bit* bit, const void* data, size_t size);

const char* bit_get_error_string(struct bit* bit);

#endif // _BIT_H
