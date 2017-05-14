#include <cha.h>

#include <string.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

static int cha_switch_mode(struct cha* cha, unsigned char mode) {
	if (ftdi_set_bitmode(&cha->ftdi, 0, BITMODE_RESET) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_reset;
	}

	if (ftdi_set_bitmode(&cha->ftdi, 0xFF, mode) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_set;
	}

	return 0;

fail_ftdi_set_bitmode_set:
fail_ftdi_set_bitmode_reset:
	return -1;
}

int cha_init(struct cha* cha) {
	memset(cha, 0, sizeof(struct cha));

	if (ftdi_init(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_init;
	}

	if (ftdi_set_interface(&cha->ftdi, INTERFACE_A) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_interface;
	}

	return 0;

fail_ftdi_set_interface:
	ftdi_deinit(&cha->ftdi);
fail_ftdi_init:
	return -1;
}

int cha_open(struct cha* cha) {
	if (ftdi_usb_open(&cha->ftdi, OV_VENDOR, OV_PRODUCT) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_usb_open;
	}

	if (ftdi_usb_reset(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_usb_reset;
	}
	
	if (cha_switch_config_mode(cha) == -1) {
		goto fail_switch_config_mode;
	}

	return 0;

fail_switch_config_mode:
fail_ftdi_usb_reset:
	ftdi_usb_close(&cha->ftdi);
fail_ftdi_usb_open:
	return -1;
}

int cha_switch_config_mode(struct cha* cha) {
	return cha_switch_mode(cha, BITMODE_BITBANG);
}

int cha_switch_fifo_mode(struct cha* cha) {
	return cha_switch_mode(cha, BITMODE_SYNCFF);
}

void cha_destroy(struct cha* cha) {
	ftdi_deinit(&cha->ftdi);
}

const char* cha_get_error_string(struct cha* cha) {
	return cha->error_str;
}
