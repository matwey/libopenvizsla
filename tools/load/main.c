#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bit.h>
#include <fwpkg.h>
#include <cha.h>
#include <chb.h>

#define PORTB_DONE_BIT     (1 << 2)  // GPIOH2
#define PORTB_INIT_BIT     (1 << 5)  // GPIOH5

int print_status(struct chb* chb) {
	int ret;
	uint8_t status;

	ret = chb_get_status(chb, &status);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(chb));
		return -1;
	}

	printf("Device status: %02x %s %s\n", status, status & PORTB_INIT_BIT ? "init" : "", status & PORTB_DONE_BIT ? "done" : "");

	return 0;
}

int main(int argc, char** argv) {
	int ret;
	size_t size;
	uint8_t* firmware;
	struct fwpkg* fwpkg = fwpkg_new();
	struct bit bit;
	struct cha cha;
	struct chb chb;
	
	ret = fwpkg_from_preload(fwpkg);
	if (ret == -1) {
		fprintf(stderr, fwpkg_get_error_string(fwpkg));
		return 1;
	}

	size = fwpkg_bitstream_size(fwpkg);
	firmware = malloc(size);
	if (firmware == NULL) {
		fprintf(stderr, "Cannot allocate memory for firmware %d", size);
		return 1;
	}

	ret = fwpkg_read_bitstream(fwpkg, firmware, &size);

	if (ret == -1) {
		fprintf(stderr, fwpkg_get_error_string(fwpkg));
		return 1;
	}

	if (bit_init(&bit, firmware, size) < 0) {
		fprintf(stderr, bit_get_error_string(&bit));
		return 1;
	}

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

	if (print_status(&chb) < 0)
		return 1;

	if (cha_switch_config_mode(&cha) < 0) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	if (chb_switch_program_mode(&chb) < 0) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	if (cha_load_firmware(&cha, (uint8_t*)bit.data, bit.bit_length) < 0) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	if (cha_switch_fifo_mode(&cha) < 0) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	if (print_status(&chb) < 0)
		return 1;

	chb_destroy(&chb);
	cha_destroy(&cha);
	fwpkg_free(fwpkg);
	free(firmware);

	return 0;
}
