/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <chb.h>

#include <string.h>
#include <unistd.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

#define PORTB_TCK_BIT  (1 << 0)
#define PORTB_TDI_BIT  (1 << 1)
#define PORTB_TDO_BIT  (1 << 2)
#define PORTB_TMS_BIT  (1 << 3)

#define PORTB_CSI_BIT  (1 << 0)
#define PORTB_RDWR_BIT (1 << 1)
#define PORTB_DONE_BIT (1 << 2)
#define PORTB_PROG_BIT (1 << 3)
#define PORTB_INIT_BIT (1 << 5)
#define PORTB_M0_BIT   (1 << 6)
#define PORTB_M1_BIT   (1 << 7)

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

static int chb_tck_divisor(struct chb* chb, uint16_t value) {
	return chb_set(chb, TCK_DIVISOR, value & 0xFF, value >> 8);
}

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

	/*
	 * Configure FTDI MPSSE according to AN_135 "FTDI MPSSE Basics"
	 * 4.2 Configure FTDI Port For MPSSE Use
	 */

	if (ftdi_usb_reset(&chb->ftdi) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_usb_reset;
	}

	/* Stick with default transfer size (4096) */

	/* Disable event and error characters */
	if (ftdi_set_event_char(&chb->ftdi, 0, 0) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_event_char;
	}

	if (ftdi_set_error_char(&chb->ftdi, 0, 0) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_error_char;
	}

	/* Stick with no timeouts */
	/* Keep latency timeout intact */

	if (ftdi_setflowctrl(&chb->ftdi, SIO_RTS_CTS_HS) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_setflowctrl;
	}

	if (ftdi_set_bitmode(&chb->ftdi, 0, BITMODE_RESET) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_bitmode_reset;
	}

	if (ftdi_set_bitmode(&chb->ftdi, 0, BITMODE_MPSSE) < 0) {
		chb->error_str = ftdi_get_error_string(&chb->ftdi);
		goto fail_ftdi_set_bitmode_mpsse;
	}

	if (chb_tck_divisor(chb, 0) < 0) {
		goto fail_chb_tck_divisor;
	}

	/*
	 * Low part of PORT B is routed to JTAG
	 * During non-JTAG operation, TMS is recommended to be driven high.
	 */
	if (chb_set_low(chb, PORTB_TMS_BIT) < 0) {
		goto fail_set_low;
	}
  
	return 0;

fail_set_low:
fail_chb_tck_divisor:
fail_ftdi_set_bitmode_mpsse:
fail_ftdi_set_bitmode_reset:
fail_setflowctrl:
fail_ftdi_set_error_char:
fail_ftdi_set_event_char:
fail_ftdi_usb_reset:
	ftdi_usb_close(&chb->ftdi);
fail_ftdi_usb_open:
	return -1;
}

void chb_destroy(struct chb* chb) {
	ftdi_deinit(&chb->ftdi);
}

int chb_set_low(struct chb* chb, uint8_t val) {
	/*
	 * BD0 (TCK) output
	 * BD1 (TDI) output
	 * BD2 (TDO) input
	 * BD3 (TMS) output
	 */
	return chb_set(chb, SET_BITS_LOW, val, PORTB_TCK_BIT | PORTB_TDI_BIT | PORTB_TMS_BIT);
}

int chb_set_high(struct chb* chb, uint8_t val) {
	/*
	 * BC0 (CSI)  output active-low
	 * BC1 (RDWR) output active-low
	 * BC2 (DONE) input  active-high
	 * BC3 (PROG) output
	 * BC5 (INIT) input  active-low
	 * BC6 (M0)   output
	 * BC7 (M1)   output
	 */
	return chb_set(chb, SET_BITS_HIGH, val, PORTB_CSI_BIT | PORTB_RDWR_BIT | PORTB_M1_BIT | PORTB_PROG_BIT | PORTB_M0_BIT);
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

int chb_get_status(struct chb* chb, uint8_t* status) {
	if (chb_get_high(chb, status) < 0)
		return -1;

	return 0;
}

int chb_switch_program_mode(struct chb* chb) {
	int ret;
	uint8_t status;
	int try = 3;

	// Mode pins       M[1:0] = 0b10 (active-low)
	if (chb_set_high(chb, PORTB_CSI_BIT | PORTB_RDWR_BIT | PORTB_PROG_BIT | PORTB_M1_BIT) < 0)
		return -1;

	// Full-chip reset PROG low
	if (chb_set_high(chb, PORTB_CSI_BIT | PORTB_M1_BIT) < 0)
		return -1;

	// Full-chip reset PROG high
	if (chb_set_high(chb, PORTB_PROG_BIT | PORTB_M1_BIT) < 0)
		return -1;

	for (try = 3; try && (ret = chb_get_high(chb, &status)) == 0 && (status & PORTB_DONE_BIT); --try) {
		usleep(10000);
	}

	if (ret < 0)
		return -1;

	if (try == 0 && (status & PORTB_DONE_BIT)) {
		chb->error_str = "done bit stuck high after going into programming mode";
		return -1;
	}

	return 0;
}

const char* chb_get_error_string(struct chb* chb) {
	return chb->error_str;
}
