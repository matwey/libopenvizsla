/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <ov.h>

#include <cha.h>
#include <chb.h>
#include <fwpkg.h>

#include <stdlib.h>

struct ov_device {
	struct fwpkg fwpkg;
	struct cha cha;
	struct chb chb;
	struct cha_loop loop;
	const char* error_str;
};

int ov_init(struct ov_device* ov) {
	int ret = 0;
	char* map = NULL;
	size_t map_size = 0;

	ret = fwpkg_from_preload(&ov->fwpkg);
	if (ret < 0) {
		ov->error_str = fwpkg_get_error_string(&ov->fwpkg);
		goto error_fwpkg_from_preload;
	}

	map_size = fwpkg_map_size(&ov->fwpkg)+1;
	map = malloc(map_size);
	if (map == NULL) {
		ov->error_str = "Cannot allocate memory for register map";
		goto error_malloc;
	}

	ret = fwpkg_read_map(&ov->fwpkg, map, &map_size);
	if (ret < 0) {
		ov->error_str = fwpkg_get_error_string(&ov->fwpkg);
		goto error_fwpkg_read_map;
	}
	map[map_size] = '\0';

	ret = cha_init(&ov->cha, map);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto error_cha_init;
	}

	free(map);
	map = NULL;

	ret = chb_init(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto error_chb_init;
	}

	return 0;

error_chb_init:
	cha_destroy(&ov->cha);
error_cha_init:
error_fwpkg_read_map:
	if (map) {
		free(map);
	}
error_malloc:
	fwpkg_free(&ov->fwpkg);
error_fwpkg_from_preload:
	return ret;
}

int ov_open(struct ov_device* ov) {
	int ret = 0;

	ret = cha_open(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto error_cha_open;
	}

	ret = chb_open(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto error_chb_open;
	}

	return 0;

error_chb_open:
	// FIXME: close cha?
error_cha_open:

	return ret;
}

void ov_destroy(struct ov_device* ov) {
	chb_destroy(&ov->chb);
	cha_destroy(&ov->cha);
	fwpkg_free(&ov->fwpkg);
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

	if (cha_switch_fifo_mode(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_switch_fifo_mode;
	}

	if (cha_stop_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_stop_stream;
	}

	if (cha_write_reg(&ov->cha, 0x402, 0x4a) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_ucfg_wdata_wr;
	}

	if (cha_write_reg(&ov->cha, 0x403, 0x80 | (0x04 & 0x3F)) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_ucfg_wcmd_wr;
	}

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
