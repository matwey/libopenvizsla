#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ftdi.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

#define PORTB_DONE_BIT     (1 << 2)  // GPIOH2
#define PORTB_INIT_BIT     (1 << 5)  // GPIOH5

int main(int argc, char** argv) {
	unsigned char buf[] = {0x55, 0x04, 0x01, 0x00, 0x5a};
	unsigned char inp_buf[60];
	int ret;

	struct ftdi_context *ftdi_a, *ftdi_b;
	struct ftdi_version_info version;

	ftdi_a = ftdi_new();
	if (!ftdi_a) {
		perror("ftdi_new");
		return 1;
	}

	ftdi_b = ftdi_new();
	if (!ftdi_b) {
		perror("ftdi_new");
		return 1;
	}

	version = ftdi_get_library_version();
	printf("Hello %s %s\n", version.version_str, version.snapshot_str);

	ret = ftdi_set_interface(ftdi_a, INTERFACE_A);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_interface: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}


	ret = ftdi_usb_open(ftdi_a, OV_VENDOR, OV_PRODUCT);
	if (ret < 0) {
		fprintf(stderr, "ftdi_usb_open: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	ret = ftdi_usb_reset(ftdi_a);
	if (ret < 0) {
		fprintf(stderr, "ftdi_usb_reset: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	ret = ftdi_set_interface(ftdi_b, INTERFACE_B);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_interface: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	ret = ftdi_usb_open(ftdi_b, OV_VENDOR, OV_PRODUCT);
	if (ret < 0) {
		fprintf(stderr, "ftdi_usb_open: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	ret = ftdi_usb_reset(ftdi_b);
	if (ret < 0) {
		fprintf(stderr, "ftdi_usb_reset: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	ret = ftdi_set_bitmode(ftdi_a, 0xFF, BITMODE_BITBANG);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	ret = ftdi_set_baudrate(ftdi_a, 1000000);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_baudrate: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	ret = ftdi_set_bitmode(ftdi_b, 0, BITMODE_RESET);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	ret = ftdi_set_bitmode(ftdi_b, SIO_SET_BITMODE_REQUEST, BITMODE_MPSSE);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	uint8_t mpsse_set_high[3] = {SET_BITS_HIGH, 0, 0};
	ret = ftdi_write_data(ftdi_b, mpsse_set_high, sizeof(mpsse_set_high));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	uint8_t mpsse_get_high[1] = {GET_BITS_HIGH};
	ret = ftdi_write_data(ftdi_b, mpsse_get_high, sizeof(mpsse_get_high));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	uint8_t mpsse_get_high_buf[3];
	ret = ftdi_read_data(ftdi_b, mpsse_get_high_buf, sizeof(mpsse_get_high_buf));
	if (ret < 0) {
		fprintf(stderr, "ftdi_read_data: %s\n", ftdi_get_error_string(ftdi_b));
		return 1;
	}

	printf("%d %x %d %d\n", ret, mpsse_get_high_buf[0], mpsse_get_high_buf[0] & PORTB_INIT_BIT, mpsse_get_high_buf[0] & PORTB_DONE_BIT);

	ret = ftdi_set_bitmode(ftdi_a, 0xFF, BITMODE_SYNCFF);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	uint8_t init_cycles[512];
	memset(init_cycles, 0, sizeof(init_cycles));
	ret = ftdi_write_data(ftdi_a, init_cycles, sizeof(init_cycles));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	uint8_t cmd_send[5] = {0x55, 0x04, 0x01, 0x00, 0x5a};
	ret = ftdi_write_data(ftdi_a, cmd_send, sizeof(cmd_send));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	do {
	ret = ftdi_read_data(ftdi_a, cmd_send, sizeof(cmd_send));
	if (ret < 0) {
		fprintf(stderr, "ftdi_read_data: %s\n", ftdi_get_error_string(ftdi_a));
		return 1;
	}

	printf("%d ", ret);
	for (size_t i = 0; i < ret; i++)
		printf("%02x ", cmd_send[i]);
	printf("\n");
	} while (ret == 0);

	ftdi_usb_close(ftdi_b);
	ftdi_usb_close(ftdi_a);

	ftdi_free(ftdi_b);
	ftdi_free(ftdi_a);

	return 0;
}
