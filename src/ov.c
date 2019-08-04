/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <ov.h>

#include <cha.h>
#include <chb.h>
#include <bit.h>
#include <fwpkg.h>

#include <stdlib.h>

#define PORTB_DONE_BIT     (1 << 2)  // GPIOH2
#define PORTB_INIT_BIT     (1 << 5)  // GPIOH5

struct ov_device {
	struct cha cha;
	struct chb chb;
	struct fwpkg fwpkg;
	struct cha_loop loop;
	const char* error_str;
};

/* cha must be in config mode to operate */
static int ov_get_device_status(struct ov_device* ov) {
	const uint8_t mask = PORTB_INIT_BIT | PORTB_DONE_BIT;
	uint8_t status = 0;
	int ret = 0;

	ret = chb_get_status(&ov->chb, &status);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		return -1;
	}

	return (status & mask) == mask;
}

static int ov_load_firmware_from_fwpkg(struct ov_device* ov, struct fwpkg* fwpkg) {
	struct bit bit;
	size_t size;
	void* tmp = NULL;
	int ret = 0;

	size = fwpkg_bitstream_size(fwpkg);
	tmp = malloc(size);
	if (!tmp) {
		ov->error_str = "Cannot allocate memory for firmware bitstream";
		goto fail_malloc;
	}

	ret = fwpkg_read_bitstream(fwpkg, tmp, &size);
	if (ret < 0) {
		ov->error_str = fwpkg_get_error_string(fwpkg);
		goto fail_fwpkg_read_bitstream;
	}

	ret = bit_init(&bit, tmp, size);
	if (ret < 0) {
		ov->error_str = bit_get_error_string(&bit);
		goto fail_bit_init;
	}

	ret = cha_switch_config_mode(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_switch_config_mode;
	}

	ret = chb_switch_program_mode(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto fail_chb_switch_program_mode;
	}

	ret = bit_load_firmware(&bit, &ov->cha, &ov->chb);
	if (ret < 0) {
		ov->error_str = bit_get_error_string(&bit);
		goto fail_bit_load_firmware;
	}

	free(tmp);
	tmp = NULL;

	ret = cha_switch_fifo_mode(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_switch_fifo_mode;
	}

	return 0;

fail_cha_switch_fifo_mode:
fail_bit_load_firmware:
fail_chb_switch_program_mode:
	cha_switch_fifo_mode(&ov->cha);
fail_cha_switch_config_mode:
fail_bit_init:
fail_fwpkg_read_bitstream:
	if (tmp) {
		free(tmp);
	}
fail_malloc:

	return -1;
}

struct ov_device* ov_new(const char* firmware_filename) {
	struct ov_device* ov = NULL;
	int ret = 0;

	ov = malloc(sizeof(struct ov_device));
	if (!ov) {
		goto fail_malloc;
	}

	memset(ov, 0, sizeof(struct ov_device));

	ret = fwpkg_init(&ov->fwpkg, firmware_filename);
	if (ret < 0) {
		ov->error_str = fwpkg_get_error_string(&ov->fwpkg);
		goto fail_fwpkg_init;
	}

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
	fwpkg_destroy(&ov->fwpkg);
fail_fwpkg_init:
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

	ret = ov_get_device_status(ov);
	if (ret < 0) {
		goto fail_ov_get_device_status;
	} else if (ret == 0) {
		ret = ov_load_firmware_from_fwpkg(ov, &ov->fwpkg);
		if (ret < 0) {
			goto fail_ov_load_firmware;
		}
	}

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
fail_ov_load_firmware:
fail_ov_get_device_status:
fail_chb_open:
	// FIXME: close cha?
fail_cha_open:

	return ret;
}

void ov_free(struct ov_device* ov) {
	chb_destroy(&ov->chb);
	cha_destroy(&ov->cha);
	fwpkg_destroy(&ov->fwpkg);
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

int ov_load_firmware(struct ov_device* ov, const char* filename) {
	int ret = 0;
	struct fwpkg fwpkg;

	ret = fwpkg_init(&fwpkg, filename);
	if (ret < 0) {
		ov->error_str = fwpkg_get_error_string(&fwpkg);
		goto fail_fwpkg_init;
	}

	ret = ov_load_firmware_from_fwpkg(ov, &fwpkg);
	if (ret < 0) {
		goto fail_ov_load_firmware;
	}

	fwpkg_destroy(&fwpkg);

	return 0;

fail_ov_load_firmware:
	fwpkg_destroy(&fwpkg);
fail_fwpkg_init:

	return -1;
}

const char* ov_get_error_string(struct ov_device* ov) {
	return ov->error_str;
}
