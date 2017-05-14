#ifndef _CHA_H
#define _CHA_H

#include <ftdi.h>

#include <stdint.h>

struct cha {
	struct ftdi_context ftdi;
	const char* error_str;
};

int cha_init(struct cha* cha);
int cha_open(struct cha* cha);
void cha_destroy(struct cha* cha);

const char* cha_get_error_string(struct cha* cha);

#endif // _CHA_H
