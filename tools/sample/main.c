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
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	ret = chb_init(&chb);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	ret = cha_open(&cha);
	if (ret == -1) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	ret = chb_open(&chb);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	uint8_t status;

	ret = chb_set_high(&chb, 0);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	ret = chb_get_high(&chb, &status);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	printf("%x %d %d\n", status, status & PORTB_INIT_BIT, status & PORTB_DONE_BIT);

	ret = cha_switch_fifo_mode(&cha);
	if (ret == -1) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	/* All leds on */
	ret = cha_write_reg(&cha, 0x0, 0x07);
	if (ret == -1) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	chb_destroy(&chb);
	cha_destroy(&cha);

	return 0;
}
