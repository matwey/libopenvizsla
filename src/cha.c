/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <cha.h>
#include <decoder.h>

#include <assert.h>
#include <string.h>

#include <libusb.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

#define UCFG_REG_ADDRMASK 0x3f
#define UCFG_REG_GO 0x80

/* libftdi < 1.5rc1 has the old API, and we can detect that by the presence of the
 * SIO_TCIFLUSH macro */
#ifndef SIO_TCIFLUSH
#define ftdi_tcioflush(x) ftdi_usb_purge_buffers(x)
#endif

struct cha_loop_packet_callback_state {
	size_t count;
	ov_packet_decoder_callback user_callback;
	void* user_data;
};

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

	if (ftdi_tcioflush(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_tcioflush;
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

	if (ftdi_tcioflush(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_tcioflush;
	}

	return 0;

fail_ftdi_tcioflush:
fail_ftdi_read_data:
fail_ftdi_write_data:
	return -1;
}

static int cha_transaction(struct cha* cha, uint16_t addr, uint8_t in_val, uint8_t* out_val) {
	uint8_t msg[5] = {0x55, addr >> 8, addr & 0xFF, in_val, 0x00};
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

	if (out_val)
		*out_val = msg[3];

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

	if (ftdi_set_baudrate(&cha->ftdi, 315000) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_set_baudrate;
	}

	if (ftdi_tcioflush(&cha->ftdi) < 0) {
		cha->error_str = ftdi_get_error_string(&cha->ftdi);
		goto fail_ftdi_tcioflush;
	}

	return 0;

fail_ftdi_tcioflush:
fail_ftdi_set_bitmode_set:
fail_ftdi_set_baudrate:
fail_ftdi_set_bitmode_reset:
	return -1;
}

int cha_init(struct cha* cha, struct fwpkg* fwpkg) {
	int ret = 0;

	memset(cha, 0, sizeof(struct cha));

	ret = reg_init_from_fwpkg(&cha->reg, fwpkg);
	if (ret < 0) {
		cha->error_str = reg_get_error_string(&cha->reg);
		goto fail_reg_init_from_fwpkg;
	}

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
fail_reg_init_from_fwpkg:

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
	if (cha_sync_stream(cha, 0x8000 | cha->reg.addr[SDRAM_HOST_READ_GO]) < 0) {
		goto fail_cha_sync_stream;
	}

	return 0;

fail_cha_sync_stream:
fail_ftdi_write_data:
fail_switch_mode:
	return -1;
}

static int cha_write_reg(struct cha* cha, uint16_t addr, uint8_t val) {
	return cha_transaction(cha, addr | 0x8000, val, NULL);
}

static int cha_read_reg(struct cha* cha, uint16_t addr, uint8_t* val) {
	return cha_transaction(cha, addr, 0, val);
}

static int cha_write_reg32(struct cha* cha, uint16_t addr, uint32_t val) {
	for (uint16_t i = addr + 4; i != addr; --i, val = (val >> 8)) {
		if (cha_write_reg(cha, i - 1, val & 0xFF) == -1)
			return -1;
	}

	return 0;
}

static int cha_read_reg32(struct cha* cha, uint16_t addr, uint32_t* val) {
	uint8_t tmp = 0;

	for (uint16_t i = addr; i != addr + 4; ++i) {
		if (cha_read_reg(cha, i, &tmp) == -1)
			return -1;

		*val = (*val << 8) | tmp;
	}

	return 0;
}

int cha_write_reg_by_name(struct cha* cha, enum reg_name name, uint8_t val) {
	return cha_write_reg(cha, cha->reg.addr[name], val);
}

int cha_read_reg_by_name(struct cha* cha, enum reg_name name, uint8_t* val) {
	return cha_read_reg(cha, cha->reg.addr[name], val);
}

int cha_write_reg32_by_name(struct cha* cha, enum reg_name name, uint32_t val) {
	return cha_write_reg32(cha, cha->reg.addr[name], val);
}

int cha_read_reg32_by_name(struct cha* cha, enum reg_name name, uint32_t* val) {
	return cha_read_reg32(cha, cha->reg.addr[name], val);
}

int cha_write_ulpi(struct cha* cha, uint8_t addr, uint8_t val) {
	int ret = 0;
	uint8_t tmp = 0;

	ret = cha_write_reg_by_name(cha, UCFG_WDATA, val);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, UCFG_WCMD, UCFG_REG_GO | (addr & UCFG_REG_ADDRMASK));
	if (ret == -1)
		return ret;

	do {
		ret = cha_read_reg_by_name(cha, UCFG_WCMD, &tmp);
		if (ret == -1)
			return ret;
	} while(tmp & UCFG_REG_GO);

	return 0;
}

int cha_read_ulpi(struct cha* cha, uint8_t addr, uint8_t* val) {
	int ret = 0;
	uint8_t tmp = 0;

	ret = cha_write_reg_by_name(cha, UCFG_RCMD, UCFG_REG_GO | (addr & UCFG_REG_ADDRMASK));
	if (ret == -1)
		return ret;

	do {
		ret = cha_read_reg_by_name(cha, UCFG_RCMD, &tmp);
		if (ret == -1)
			return ret;
	} while(tmp & UCFG_REG_GO);

	ret = cha_read_reg_by_name(cha, UCFG_RDATA, val);
	if (ret == -1)
		return ret;

	return 0;
}

int cha_get_usb_speed(struct cha* cha, enum ov_usb_speed* speed) {
	int ret = 0;
	uint8_t wdata = 0;

	ret = cha_read_ulpi(cha, 0x04, &wdata);
	if (ret == -1)
		return ret;

	switch (wdata) {
	case 0x4a: {
		*speed = OV_LOW_SPEED;
	} break;
	case 0x49: {
		*speed = OV_FULL_SPEED;
	} break;
	case 0x48: {
		*speed = OV_HIGH_SPEED;
	} break;
	default: {
		cha->error_str = "Invalid USB speed code";

		return -1;
	} break;
	}

	return 0;
}

int cha_set_usb_speed(struct cha* cha, enum ov_usb_speed speed) {
	int ret = 0;
	uint8_t wdata = 0;

	switch (speed) {
	case OV_LOW_SPEED: {
		wdata = 0x4a;
	} break;
	case OV_FULL_SPEED: {
		wdata = 0x49;
	} break;
	case OV_HIGH_SPEED: {
		wdata = 0x48;
	} break;
	}

	// self.regs.ucfg_wdata.wr(value)
	ret = cha_write_ulpi(cha, 0x04, wdata);
	if (ret == -1)
		return ret;

	return 0;
}

int cha_start_stream(struct cha* cha) {
	int ret = 0;

	ret = cha_write_reg32_by_name(cha, SDRAM_SINK_RING_BASE, 0);
	if (ret == -1)
		return ret;

	ret = cha_write_reg32_by_name(cha, SDRAM_SINK_RING_END, 0x01000000);
	if (ret == -1)
		return ret;

	ret = cha_write_reg32_by_name(cha, SDRAM_HOST_READ_RING_BASE, 0);
	if (ret == -1)
		return ret;

	ret = cha_write_reg32_by_name(cha, SDRAM_HOST_READ_RING_END, 0x01000000);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, SDRAM_SINK_PTR_READ, 0);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, SDRAM_SINK_GO, 1);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, SDRAM_HOST_READ_GO, 1);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, CSTREAM_CFG, 1);
	if (ret == -1)
		return ret;

	return 0;
}

int cha_stop_stream(struct cha* cha) {
	int ret = 0;

	ret = cha_write_reg_by_name(cha, SDRAM_HOST_READ_GO, 0);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, SDRAM_SINK_GO, 0);
	if (ret == -1)
		return ret;

	ret = cha_write_reg_by_name(cha, CSTREAM_CFG, 0);
	if (ret == -1)
		return ret;

	return 0;
}

static void cha_loop_packet_callback(struct ov_packet* packet, void* data) {
	struct cha_loop* loop = (struct cha_loop*)data;

	/* When the loop is stopped via cha_loop_break(), leftover packets may
	 * follow from the read data buffer. */
	if (loop->break_loop)
		return;

	if (loop->max_count > 0)
		loop->count++;

	if (loop->callback) {
		loop->callback(packet, loop->user_data);
	}
}

static void cha_loop_free_transfer(struct libusb_transfer* transfer) {
	struct cha_loop* loop = (struct cha_loop*)transfer->user_data;

	libusb_free_transfer(transfer);
	loop->complete = 1;
}

static void LIBUSB_CALL cha_loop_transfer_callback(struct libusb_transfer* transfer) {
	struct cha_loop* loop = (struct cha_loop*)transfer->user_data;
	struct cha* cha = loop->cha;

	int ret = 0;

	switch (transfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: {
			const int done = (loop->max_count > 0 && loop->count >= loop->max_count);

			if (transfer->actual_length > 2) {
				/* Skip FTDI header */
				if (frame_decoder_proc(
					&loop->fd,
					transfer->buffer + 2,
					transfer->actual_length - 2) < 0) {

					loop->break_loop = 1;
					cha->error_str = loop->fd.error_str;
				};
			}

			if (!(done || loop->break_loop) && (ret = libusb_submit_transfer(transfer)) < 0) {
				if (ret != LIBUSB_ERROR_INTERRUPTED) {
					cha->error_str = libusb_error_name(ret);
				}

				loop->break_loop = 1;
			}

			if (done || loop->break_loop) {
				cha_loop_free_transfer(transfer);
			}
		} break;
		case LIBUSB_TRANSFER_CANCELLED: {
			cha_loop_free_transfer(transfer);
		} break;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_TIMED_OUT:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_NO_DEVICE:
		case LIBUSB_TRANSFER_OVERFLOW:
		default: {
			cha->error_str = libusb_error_name(transfer->status);
			loop->break_loop = 1;

			cha_loop_free_transfer(transfer);
		} break;
	}
}

static int cha_loop_read_from_ftdi(struct cha_loop* loop) {
	struct ftdi_context* ftdi = &loop->cha->ftdi;
	int ret = 0;

	if ((ret = frame_decoder_proc(
		&loop->fd,
		ftdi->readbuffer + ftdi->readbuffer_offset,
		ftdi->readbuffer_remaining)) < 0) {

		loop->cha->error_str = loop->fd.error_str;
		goto fail_decode_ftdi_readbuffer;
	}

	assert(ret == ftdi->readbuffer_remaining);

	ftdi->readbuffer_remaining -= ret;
	ftdi->readbuffer_offset = 0;

	return 0;

fail_decode_ftdi_readbuffer:
	return -1;
}

int cha_loop_init(struct cha_loop* loop, struct cha* cha, struct ov_packet* packet, size_t packet_size, ov_packet_decoder_callback callback, void* user_data) {
	loop->cha = cha;
	loop->callback = callback;
	loop->user_data = user_data;

	if (frame_decoder_init(&loop->fd, packet, packet_size, &cha_loop_packet_callback, loop) < 0) {
		cha->error_str = "Frame decoder init failure";
		goto fail_frame_decode_init;
	}

	return 0;

fail_frame_decode_init:
	return -1;
}

int cha_loop_run(struct cha_loop* loop, int count) {
	struct cha* cha = loop->cha;
	struct ftdi_context* ftdi = &cha->ftdi;

	struct libusb_transfer* usb_transfer;
	int ret = 0;

	loop->count = 0;
	loop->max_count = count;
	loop->break_loop = 0;
	loop->complete = 0;

	if (cha_loop_read_from_ftdi(loop) < 0) {
		goto fail_read_from_ftdi;
	}

	usb_transfer = libusb_alloc_transfer(0);
	if (!usb_transfer) {
		cha->error_str = "Can not allocate libusb_transfer";
		goto fail_libusb_alloc_transfer;
	}

	libusb_fill_bulk_transfer(usb_transfer, ftdi->usb_dev, ftdi->out_ep, ftdi->readbuffer, ftdi->max_packet_size, &cha_loop_transfer_callback, loop, 100);

	if ((ret = libusb_submit_transfer(usb_transfer)) < 0) {
		cha->error_str = libusb_error_name(ret);
		goto fail_libusb_submit_transfer;
	}

	do {
		if ((ret = libusb_handle_events_completed(ftdi->usb_ctx, &loop->complete)) < 0) {
			loop->break_loop = 1;

			if (ret != LIBUSB_ERROR_INTERRUPTED) {
				cha->error_str = libusb_error_name(ret);
			}
		}
	} while (!loop->complete);

	if (loop->break_loop) {
		if (cha->error_str)
			return -1;
		return -2;
	}

	return loop->count;

fail_libusb_submit_transfer:
	libusb_free_transfer(usb_transfer);
fail_libusb_alloc_transfer:
fail_read_from_ftdi:
	return -1;
}

ov_packet_decoder_callback cha_loop_set_callback(struct cha_loop* loop, ov_packet_decoder_callback callback, void* user_data) {
	ov_packet_decoder_callback old_callback = loop->callback;

	loop->callback = callback;
	loop->user_data = user_data;

	return old_callback;
}

void cha_loop_break(struct cha_loop* loop) {
	loop->break_loop = 1;
}

int cha_set_reg(struct cha* cha, struct reg* reg) {
	int ret = 0;

	ret = reg_init_from_reg(&cha->reg, reg);
	if (ret < 0) {
		cha->error_str = reg_get_error_string(&cha->reg);
		return -1;
	}

	return 0;
}

void cha_destroy(struct cha* cha) {
	ftdi_deinit(&cha->ftdi);
}

const char* cha_get_error_string(struct cha* cha) {
	return cha->error_str;
}
