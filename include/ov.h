#ifndef _OV_H
#define _OV_H

#include <cha.h>
#include <chb.h>

struct ov_device {
	struct cha cha;
	struct chb chb;
	const char* error_str;
};

int  ov_init(struct ov_device* ov);
int  ov_open(struct ov_device* ov);
void ov_destroy(struct ov_device* ov);

const char* ov_get_error_string(struct ov_device* ov);

#endif // _OV_H
