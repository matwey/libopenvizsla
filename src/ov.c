/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <ov.h>

#include <cha.h>
#include <chb.h>

#include <stdlib.h>

struct ov_device {
	struct cha cha;
	struct chb chb;
	struct cha_loop loop;
	const char* error_str;
};

struct ov_device* ov_new(void) {
	struct ov_device* ov = NULL;
	int ret = 0;

	ov = malloc(sizeof(struct ov_device));
	if (!ov) {
		goto fail_malloc;
	}

	memset(ov, 0, sizeof(struct ov_device));

	ret = cha_init(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_init;
	}

	ret = chb_init(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto fail_chb_init;
	}

	return ov;

fail_chb_init:
	cha_destroy(&ov->cha);
fail_cha_init:
	free(ov);
fail_malloc:

	return NULL;
}

int ov_open(struct ov_device* ov) {
	int ret = 0;

	ret = cha_open(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_open;
	}

	ret = chb_open(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto fail_chb_open;
	}

	/* TODO: Check the board status and upload firmware if required */

	ret = cha_switch_fifo_mode(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_switch_fifo_mode;
	}

	ret = cha_stop_stream(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_stop_stream;
	}

	return 0;

fail_cha_stop_stream:
fail_cha_switch_fifo_mode:
fail_chb_open:
	// FIXME: close cha?
fail_cha_open:

	return ret;
}

void ov_free(struct ov_device* ov) {
	chb_destroy(&ov->chb);
	cha_destroy(&ov->cha);
	free(ov);
}

int ov_get_usb_speed(struct ov_device* ov, enum ov_usb_speed* speed) {
	if (cha_get_usb_speed(&ov->cha, speed) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_get_usb_speed;
	}

	return 0;

fail_cha_get_usb_speed:
	return -1;
}

int ov_set_usb_speed(struct ov_device* ov, enum ov_usb_speed speed) {
	if (cha_set_usb_speed(&ov->cha, speed) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_set_usb_speed;
	}

	return 0;

fail_cha_set_usb_speed:
	return -1;
}

int ov_capture_start(struct ov_device* ov, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data) {

	if (cha_loop_init(&ov->loop, &ov->cha, packet, packet_size, callback, user_data) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_loop_init;
	}

	if (cha_start_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_start_stream;
	}

	return 0;

fail_cha_start_stream:
fail_cha_loop_init:
fail_ucfg_wcmd_wr:
fail_ucfg_wdata_wr:
fail_cha_stop_stream:
fail_cha_switch_fifo_mode:
	return -1;
}

int ov_capture_dispatch(struct ov_device* ov, int count) {
	int ret = 0;

	if ((ret = cha_loop_run(&ov->loop, count)) == -1) {
		ov->error_str = cha_get_error_string(&ov->cha);
	}

	return ret;
}

int ov_capture_stop(struct ov_device* ov) {
	if (cha_stop_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		return -1;
	}

	return 0;
}

const char* ov_get_error_string(struct ov_device* ov) {
	return ov->error_str;
}
