#include <cha.h>
#include <decoder.h>

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

int cha_write_reg32(struct cha* cha, uint16_t addr, uint32_t val) {
	for (uint16_t i = addr + 4; i != addr; --i, val = (val >> 8)) {
		if (cha_write_reg(cha, i - 1, val & 0xFF) == -1)
			return -1;
	}

	return 0;
}

int cha_read_reg32(struct cha* cha, uint16_t addr, uint32_t* val) {
	uint8_t tmp = 0;

	for (uint16_t i = addr; i != addr + 4; ++i) {
		if (cha_read_reg(cha, i, &tmp) == -1)
			return -1;

		*val = (*val << 8) | tmp;
	}

	return 0;
}

int cha_start_stream(struct cha* cha) {
	int ret = 0;

	// dev.regs.SDRAM_SINK_RING_BASE.wr(ring_base)
	ret = cha_write_reg32(cha, 0xe09, 0);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_SINK_RING_END.wr(ring_end)
	ret = cha_write_reg32(cha, 0xe0d, 0x01000000);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_HOST_READ_RING_BASE.wr(ring_base)
	ret = cha_write_reg32(cha, 0xc1c, 0);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_HOST_READ_RING_END.wr(ring_end)
	ret = cha_write_reg32(cha, 0xc20, 0x01000000);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_SINK_PTR_READ.wr(0)
	ret = cha_write_reg(cha, 0xe00, 0);
	if (ret == -1)
		return ret;

	// dev.regs.CSTREAM_CFG.wr(1)
	ret = cha_write_reg(cha, 0x800, 1);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_SINK_GO.wr(1)
	ret = cha_write_reg(cha, 0xe11, 1);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_HOST_READ_GO.wr(1)
	ret = cha_write_reg(cha, 0xc28, 1);
	if (ret == -1)
		return ret;

	return 0;
}

int cha_stop_stream(struct cha* cha) {
	int ret = 0;

	// dev.regs.SDRAM_HOST_READ_GO.wr(0)
	// FIXME: use loaded register
	ret = cha_write_reg(cha, 0xc28, 0);
	if (ret == -1)
		return ret;

	// dev.regs.SDRAM_SINK_GO.wr(0)
	// FIXME: use loaded register
	ret = cha_write_reg(cha, 0xe11, 0);
	if (ret == -1)
		return ret;

	// dev.regs.CSTREAM_CFG.wr(0)
	// FIXME: use loaded register
	ret = cha_write_reg(cha, 0x800, 0);
	if (ret == -1)
		return ret;

	return 0;
}

static void cha_loop_packet_callback(uint8_t* buf, size_t size, void* data) {
	printf("Received %d :", size);
	for (int i = 0; i < size; ++i)
		printf(" %02x", buf[i]);
	printf("\n");
}

static void cha_loop_transfer_callback(struct libusb_transfer* transfer) {
	struct frame_decoder* fd = (struct frame_decoder*)transfer->user_data;

	libusb_submit_transfer(transfer);

	if (transfer->actual_length > 2) {
		/* Skip FTDI header */
		frame_decoder_proc(fd, transfer->buffer + 2, transfer->actual_length - 2);
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
	unsigned char libusb_buf[cha->ftdi.max_packet_size];
	unsigned char packet_buf[1024];

	struct frame_decoder fd;

	if (frame_decoder_init(&fd, packet_buf, sizeof(packet_buf), &cha_loop_packet_callback, NULL) == -1)
		return -1;

	usb_transfer = libusb_alloc_transfer(0);
	if (!usb_transfer) {
		cha->error_str = "Can not allocate libusb_transfer";
		goto fail_libusb_alloc_transfer;
	}

	libusb_fill_bulk_transfer(usb_transfer, cha->ftdi.usb_dev, cha->ftdi.out_ep, libusb_buf, cha->ftdi.max_packet_size, &cha_loop_transfer_callback, &fd, 100);

	if ((ret = libusb_submit_transfer(usb_transfer)) < 0) {
		cha->error_str = libusb_error_name(ret);
		goto fail_libusb_submit_transfer;
	}

	while (cnt) {
		if ((ret = libusb_handle_events(cha->ftdi.usb_ctx)) < 0) {
			cha->error_str = libusb_error_name(ret);
			goto fail_libusb_handle_events;
		}
		if (fd.error_str) {
			cha->error_str = fd.error_str;
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
