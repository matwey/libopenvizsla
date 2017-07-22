#include <chb.h>

#include <string.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

int chb_init(struct chb* chb) {
	memset(chb, 0, sizeof(struct chb));

	if (ftdi_init(&chb->ftdi) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_init;
	}

	if (ftdi_set_interface(&chb->ftdi, INTERFACE_B) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_interface;
	}

	return 0;

fail_ftdi_set_interface:
	ftdi_deinit(&chb->ftdi);
fail_ftdi_init:
	return -1;
}

int chb_open(struct chb* chb) {
	if (ftdi_usb_open(&chb->ftdi, OV_VENDOR, OV_PRODUCT) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_usb_open;
	}

	if (ftdi_usb_reset(&chb->ftdi) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_usb_reset;
	}
	
	if (ftdi_set_bitmode(&chb->ftdi, 0, BITMODE_RESET) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_bitmode_reset;
	}

	if (ftdi_set_bitmode(&chb->ftdi, SIO_SET_BITMODE_REQUEST, BITMODE_MPSSE) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_bitmode_mpsse;
	}

	return 0;

fail_ftdi_set_bitmode_mpsse:
fail_ftdi_set_bitmode_reset:
fail_ftdi_usb_reset:
	ftdi_usb_close(&chb->ftdi);
fail_ftdi_usb_open:
	return -1;
}

void chb_destroy(struct chb* chb) {
	ftdi_deinit(&chb->ftdi);
}

static int chb_set(struct chb* chb, uint8_t cmd, uint8_t val, uint8_t mask) {
	const uint8_t mpsse_set_high[3] = {cmd, val, mask};

	if (ftdi_write_data(&chb->ftdi, mpsse_set_high, sizeof(mpsse_set_high)) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_write_data;
	}

	return 0;

fail_ftdi_write_data:
	return -1;
}

int chb_set_low(struct chb* chb, uint8_t val, uint8_t mask) {
	return chb_set(chb, SET_BITS_LOW, val, mask);
}

int chb_set_high(struct chb* chb, uint8_t val, uint8_t mask) {
	return chb_set(chb, SET_BITS_HIGH, val, mask);
}

static int chb_get(struct chb* chb, uint8_t* val, uint8_t cmd) {
	const uint8_t mpsse_get_high[1] = {cmd};

	if (ftdi_write_data(&chb->ftdi, mpsse_get_high, sizeof(mpsse_get_high)) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_write_data;
	}

	if (ftdi_read_data(&chb->ftdi, val, 1) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_read_data;
	}

	return 0;

fail_ftdi_read_data:
fail_ftdi_write_data:
	return -1;
}

int chb_get_low(struct chb* chb, uint8_t* val) {
	return chb_get(chb, val, GET_BITS_LOW);
}

int chb_get_high(struct chb* chb, uint8_t* val) {
	return chb_get(chb, val, GET_BITS_HIGH);
}

const char* chb_get_error_string(struct chb* chb) {
	return chb->error_str;
}
