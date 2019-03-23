#ifndef _OV_H
#define _OV_H

#include <cha.h>
#include <chb.h>

struct ov_device {
	struct cha cha;
	struct chb chb;
	struct cha_loop loop;
	const char* error_str;
};

int  ov_init(struct ov_device* ov);
int  ov_open(struct ov_device* ov);
void ov_destroy(struct ov_device* ov);

int ov_capture_start(struct ov_device* ov, struct packet* packet, size_t packet_size, packet_decoder_callback callback, void* user_data);
int ov_capture_dispatch(struct ov_device* ov, int count);
int ov_capture_stop(struct ov_device* ov);

const char* ov_get_error_string(struct ov_device* ov);

#endif // _OV_H
