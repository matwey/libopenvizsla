#include <cha.h>

#include <string.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

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
	
	if (ftdi_set_bitmode(&cha->ftdi, 0, BITMODE_RESET) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_reset;
	}

	if (ftdi_set_bitmode(&cha->ftdi, 0xFF, BITMODE_SYNCFF) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_mpsse;
	}

	return 0;

fail_ftdi_set_bitmode_mpsse:
fail_ftdi_set_bitmode_reset:
fail_ftdi_usb_reset:
	ftdi_usb_close(&cha->ftdi);
fail_ftdi_usb_open:
	return -1;
}

void cha_destroy(struct cha* cha) {
	ftdi_deinit(&cha->ftdi);
}

const char* cha_get_error_string(struct cha* cha) {
	return cha->error_str;
}
