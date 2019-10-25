/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ov.h>

int main(int argc, char** argv) {
	int ret;
	struct ov_device* ov;
	const char* filename = NULL;

	if (argc > 1) {
		filename = argv[1];
	}

	ov = ov_new(filename);
	if (!ov) {
		fprintf(stderr, "%s\n", "Cannot create ov_device handler");
		return 1;
	}

	ret = ov_open(ov);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot open OpenVizsla device", ov_get_error_string(ov));

		ov_free(ov);
		return 1;
	}

	ret = ov_load_firmware(ov, filename);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot load firmware", ov_get_error_string(ov));

		ov_free(ov);
		return 1;
	}

	printf("%s", "Firmware loaded\n");

	ov_free(ov);

	return 0;
}
