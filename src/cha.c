#include <cha.h>

#include <assert.h>
#include <string.h>

#include <libusb.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

static uint8_t cha_transaction_checksum(uint8_t* buf, size_t size) {
	uint8_t ret = 0;

	for (size_t i = 0; i < size; ++i) {
		ret += buf[i];
	}

	return ret;
}

static int cha_sync_stream(struct cha* cha, uint16_t addr) {
	uint8_t msg[5] = {0x55, addr >> 8, addr & 0xFF, 0x00, 0x00};
	uint8_t buf[32];
	int ret = 0, i = 0, sync_state = 0;

	msg[4] = cha_transaction_checksum(msg, 4);

	if (ftdi_usb_purge_buffers(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_usb_purge_buffers;
	}

	if (ftdi_write_data(&cha->ftdi, msg, sizeof(msg)) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_write_data;
	}

	/* FIXME: assign proper timeout to libftdi */
	do {
		if ((ret = ftdi_read_data(&cha->ftdi, buf, sizeof(buf))) < 0) {
			cha->error_str = ftdi_get_error_string(&cha->ftdi);
			goto fail_ftdi_read_data;
		}

		for (i = 0; i < ret; ++i) {
			if (buf[i] == msg[sync_state]) {
				sync_state++;
			} else {
				sync_state = 0;
			}

			if (sync_state == 5) {
				break;
			}
		}
	} while (sync_state != 5);

	if (ftdi_usb_purge_buffers(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_usb_purge_buffers;
	}

	return 0;

fail_ftdi_usb_purge_buffers:
fail_ftdi_read_data:
fail_ftdi_write_data:
	return -1;
}

static int cha_transaction(struct cha* cha, uint16_t addr, uint8_t* val) {
	uint8_t msg[5] = {0x55, addr >> 8, addr & 0xFF, *val, 0x00};
	int ret;

	msg[4] = cha_transaction_checksum(msg, 4);

	if (ftdi_write_data(&cha->ftdi, msg, sizeof(msg)) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_write_data;
	}

	/* FIXME: assign proper timeout to libftdi */
	do {
		if ((ret = ftdi_read_data(&cha->ftdi, msg, sizeof(msg))) < 0) {
			cha->error_str = ftdi_get_error_string(&cha->ftdi);
			goto fail_ftdi_read_data;
		}
	} while (ret == 0);

	if (cha_transaction_checksum(msg, 4) != msg[4]) {
		cha->error_str = "Wrong checksum";
		goto fail_transaction_checksum;
	}

	*val = msg[3];

	return 0;

fail_transaction_checksum:
fail_ftdi_read_data:
fail_ftdi_write_data:
	return -1;
}

static int cha_switch_mode(struct cha* cha, unsigned char mode) {
	if (ftdi_set_bitmode(&cha->ftdi, 0, BITMODE_RESET) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_reset;
	}

	if (ftdi_set_bitmode(&cha->ftdi, 0xFF, mode) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_bitmode_set;
	}

	if (ftdi_usb_purge_buffers(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_usb_purge_buffers;
	}

	return 0;

fail_ftdi_usb_purge_buffers:
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
	uint8_t init_cycles[512];
	memset(init_cycles, 0, sizeof(init_cycles));

	if (cha_switch_mode(cha, BITMODE_SYNCFF) == -1) {
		goto fail_switch_mode;
	}

	if (ftdi_write_data(&cha->ftdi, init_cycles, sizeof(init_cycles)) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_write_data;
	}

	/* Async FPGA-to-HOST transmission in triggered by SDRAM_HOST_READ_GO.
	 * Disable it first.
	 */
	// FIXME: use loaded register
	if (cha_sync_stream(cha, 0x8c28) < 0) {
		goto fail_cha_sync_stream;
	}

	return 0;

fail_cha_sync_stream:
fail_ftdi_write_data:
fail_switch_mode:
	return -1;
}

int cha_write_reg(struct cha* cha, uint16_t addr, uint8_t val) {
	return cha_transaction(cha, addr | 0x8000, &val);
}

int cha_read_reg(struct cha* cha, uint16_t addr, uint8_t* val) {
	return cha_transaction(cha, addr, val);
}

struct cha_loop_state {
	uint8_t* buf;
	size_t   buf_length;
	size_t   buf_actual_length;

	enum state {
		NEED_SDRAM_MARKER,
		NEED_SDRAM_LENGTH,
		NEED_SDRAM_DATA
	} state;

	size_t   required_pkg;
	size_t   required_sdram;
	enum pkg_state {
		NEED_PKG_LENGTH,
		NEED_PKG_DATA
	} pkg_state;
};

static void cha_loop_transfer_callback(struct libusb_transfer* transfer) {
	struct cha_loop_state* state = (struct cha_loop_state*)transfer->user_data;
	uint8_t* inp = transfer->buffer;
	size_t   inp_length = transfer->actual_length;

	libusb_submit_transfer(transfer);

	if (inp_length > 2) {
		inp += 2;
		inp_length -= 2;

		while (inp_length) {
			switch (state->state) {
				case NEED_SDRAM_MARKER: {
					assert(inp[0] == 0xd0);

					state->state = NEED_SDRAM_LENGTH;

					inp += 1;
					inp_length -= 1;
				} break;
				case NEED_SDRAM_LENGTH: {
					state->state = NEED_SDRAM_DATA;
					state->required_sdram = ((size_t)(inp[0])+1)*2;

					inp += 1;
					inp_length -= 1;
				} break;
				case NEED_SDRAM_DATA: {
					if (state->pkg_state == NEED_PKG_LENGTH) {
						if (state->buf_actual_length > 4) {
							state->pkg_state == NEED_PKG_DATA;
							state->required_pkg = (state->buf[4] << 8) | state->buf[3] + 8 - state->buf_actual_length;
						} else {
							state->required_pkg = 5;
						}
					}

					size_t len = (state->required_sdram < state->required_pkg ? state->required_sdram : state->required_pkg);
					len = (len < inp_length ? len : inp_length);

					memcpy(state->buf + state->buf_actual_length, inp, len);

					inp += len;
					inp_length -= len;
					state->buf_actual_length += len;
					state->required_sdram -= len;
					state->required_pkg -= len;

					if (state->pkg_state == NEED_PKG_DATA && state->required_pkg == 0) {
						state->buf_actual_length = 0;
						state->pkg_state = NEED_PKG_LENGTH;
					}

					if (state->required_sdram == 0) {
						state->state = NEED_SDRAM_MARKER;
					}
				} break;
			}
		}
	}
}

int cha_loop(struct cha* cha, int cnt) {
	/* Layout:
		FTDI pkg transfer->actual_length (less than ftdi.max_packet_size) starting with 32 60
		SDRAM frame starting with D0 XX. size = (XX + 1) * 2 // max_size = 512
		SNIFF frame starting with A0 XX YY ZZ size = (ZZ << 8 | YY) + 8
	*/
	int ret;
	struct libusb_transfer* usb_transfer;
	// FIXME:
	unsigned char buf[1024+cha->ftdi.max_packet_size];
	struct cha_loop_state state;

	state.buf = buf;
	state.buf_length = 1024;
	state.buf_actual_length = 0;
	state.state = NEED_SDRAM_MARKER;
	state.required_pkg = 0;
	state.required_sdram = 0;
	state.pkg_state = NEED_PKG_LENGTH;

	usb_transfer = libusb_alloc_transfer(0);
	if (!usb_transfer) {
		cha->error_str = "Can not allocate libusb_transfer";
		goto fail_libusb_alloc_transfer;
	}

	libusb_fill_bulk_transfer(usb_transfer, cha->ftdi.usb_dev, cha->ftdi.out_ep, buf + 1024, cha->ftdi.max_packet_size, &cha_loop_transfer_callback, &state, 100);

	if ((ret = libusb_submit_transfer(usb_transfer)) < 0) {
		cha->error_str = libusb_error_name(ret);
		goto fail_libusb_submit_transfer;
	}

	while (cnt) {
		if ((ret = libusb_handle_events(cha->ftdi.usb_ctx)) < 0) {
			cha->error_str = libusb_error_name(ret);
			goto fail_libusb_handle_events;
		}
	}

	libusb_free_transfer(usb_transfer);
	return 0;

fail_libusb_handle_events:
fail_libusb_submit_transfer:
	libusb_free_transfer(usb_transfer);
fail_libusb_alloc_transfer:
	return -1;
}

void cha_destroy(struct cha* cha) {
	ftdi_deinit(&cha->ftdi);
}

const char* cha_get_error_string(struct cha* cha) {
	return cha->error_str;
}
