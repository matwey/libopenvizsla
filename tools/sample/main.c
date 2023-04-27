/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ftdi.h>
#include <signal.h>

#include <openvizsla.h>

static void packet_handler(struct ov_packet* packet, void* data) {
	printf("[%02x] Received %d bytes at %" PRId64 ":", packet->flags, packet->size, packet->timestamp);
	for (int i = 0; i < packet->size; ++i)
		printf(" %02x", packet->data[i]);
	printf("\n");
}

static void sighandler(int signum) {
	printf("Caught signal %d\n", signum);
}

int main(int argc, char** argv) {
	int ret;
	enum ov_usb_speed speed = OV_LOW_SPEED;

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	struct ov_device* ov;
	union {
		struct ov_packet packet;
		char buf[1024];
	} p;

	ov = ov_new(NULL);
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

	ret = ov_set_usb_speed(ov, speed);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot set USB speed", ov_get_error_string(ov));

		ov_free(ov);
		return 1;
	}

	ret = ov_capture_start(ov, &p.packet, sizeof(p), &packet_handler, NULL);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot start capture", ov_get_error_string(ov));

		ov_free(ov);
		return 1;
	}

	ret = ov_capture_dispatch(ov, 100);
	if (ret == -1) {
		fprintf(stderr, "%s: %s\n", "Cannot dispatch capture", ov_get_error_string(ov));

		ov_capture_stop(ov);
		ov_free(ov);
		return 1;
	}

	ov_capture_stop(ov);

	ov_free(ov);

	return 0;
}
