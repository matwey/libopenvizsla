#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ftdi.h>

#include <cha.h>
#include <chb.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

#define PORTB_DONE_BIT     (1 << 2)  // GPIOH2
#define PORTB_INIT_BIT     (1 << 5)  // GPIOH5

int main(int argc, char** argv) {
	unsigned char buf[] = {0x55, 0x04, 0x01, 0x00, 0x5a};
	unsigned char inp_buf[60];
	int ret;

	struct cha cha;
	struct chb chb;

	struct ftdi_version_info version;

	ret = cha_init(&cha);
	if (ret == -1) {
		perror(cha_get_error_string(&cha));
		return 1;
	}

	ret = chb_init(&chb);
	if (ret == -1) {
		perror(chb_get_error_string(&chb));
		return 1;
	}

	ret = cha_open(&cha);
	if (ret == -1) {
		perror(cha_get_error_string(&cha));
		return 1;
	}

	ret = ftdi_set_bitmode(&cha.ftdi, 0xFF, BITMODE_BITBANG);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	ret = ftdi_set_baudrate(&cha.ftdi, 1000000);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_baudrate: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	ret = chb_open(&chb);
	if (ret == -1) {
		perror(chb_get_error_string(&chb));
		return 1;
	}

	uint8_t status;

	ret = chb_set_high(&chb, 0);
	if (ret == -1) {
		perror(chb_get_error_string(&chb));
		return 1;
	}

	ret = chb_get_high(&chb, &status);
	if (ret == -1) {
		perror(chb_get_error_string(&chb));
		return 1;
	}

	printf("%d %x %d %d\n", ret, status, status & PORTB_INIT_BIT, status & PORTB_DONE_BIT);

	ret = ftdi_set_bitmode(&cha.ftdi, 0xFF, BITMODE_SYNCFF);
	if (ret < 0) {
		fprintf(stderr, "ftdi_set_bitmode: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	uint8_t init_cycles[512];
	memset(init_cycles, 0, sizeof(init_cycles));
	ret = ftdi_write_data(&cha.ftdi, init_cycles, sizeof(init_cycles));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	uint8_t cmd_send[5] = {0x55, 0x04, 0x01, 0x00, 0x5a};
	ret = ftdi_write_data(&cha.ftdi, cmd_send, sizeof(cmd_send));
	if (ret < 0) {
		fprintf(stderr, "ftdi_write_data: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	do {
	ret = ftdi_read_data(&cha.ftdi, cmd_send, sizeof(cmd_send));
	if (ret < 0) {
		fprintf(stderr, "ftdi_read_data: %s\n", ftdi_get_error_string(&cha.ftdi));
		return 1;
	}

	printf("%d ", ret);
	for (size_t i = 0; i < ret; i++)
		printf("%02x ", cmd_send[i]);
	printf("\n");
	} while (ret == 0);

	chb_destroy(&chb);
	cha_destroy(&cha);

	return 0;
}
