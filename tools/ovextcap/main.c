/*
 * Copyright (c) 2019 Tomasz Moń <desowin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 199309L
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ov.h>

#define EXTCAP_VERSION_STR "0.0.1"

#define PCAP_NANOSEC_MAGIC (0xa1b23c4d)
#define LINKTYPE_USBLL (288)

#define OV_TIMESTAMP_FREQ_HZ (60000000)

/* USB packet ID is 4-bit. It is send in octet alongside complemented form.
 * The list of PIDs is available in Universal Serial Bus Specification Revision 2.0,
 * Table 8-1. PID Types
 * Packets here are sorted by the complemented form (high nibble).
 */
#define USB_PID_DATA_MDATA 0x0F
#define USB_PID_HANDSHAKE_STALL 0x1E
#define USB_PID_TOKEN_SETUP 0x2D
#define USB_PID_SPECIAL_PRE_OR_ERR 0x3C
#define USB_PID_DATA_DATA1 0x4B
#define USB_PID_HANDSHAKE_NAK 0x5A
#define USB_PID_TOKEN_IN 0x69
#define USB_PID_SPECIAL_SPLIT 0x78
#define USB_PID_DATA_DATA2 0x87
#define USB_PID_HANDSHAKE_NYET 0x96
#define USB_PID_TOKEN_SOF 0xA5
#define USB_PID_SPECIAL_PING 0xB4
#define USB_PID_DATA_DATA0 0xC3
#define USB_PID_HANDSHAKE_ACK 0xD2
#define USB_PID_TOKEN_OUT 0xE1
#define USB_PID_SPECIAL_RESERVED 0xF0

typedef struct _record_list record_list;

/** Singly linked list of queued packets. The list length is no greater than 2. */
struct _record_list {
	record_list* next;    /**< Pointer to next queued record */
	void* record;         /**< Pcap record header and packet data */
	size_t record_length; /**< Size of record in bytes */
};

struct handler_data {
	FILE* out;           /**< Output file. Set to NULL if capture stopped from Wireshark (broken pipe). */
	uint32_t utc_ts;     /**< Seconds since epoch */
	uint32_t last_ov_ts; /**< Last seen OpenVizsla timestamp. Used to detect overflows */
	uint32_t ts_offset;  /**< Timestamp offset in 1/OV_TIMESTAMP_FREQ_HZ units */

	bool filter_naks; /**< True if NAKs should be filtered */
	bool filter_sofs; /**< True if uninteresting SOFs should be filtered */
	enum { FILTER_STATE_DEFAULT,
	       FILTER_STATE_SPLIT,
	       FILTER_STATE_OUT,
	       FILTER_STATE_EXPECT_NAK,
	} st;               /**< NAK filter Finite State Machine state */
	record_list* queue; /**< Queue required for NAK filtering */
};

struct pcap_packet {
	uint32_t ts_sec;
	uint32_t ts_nsec;
	uint32_t incl_len;
	uint32_t orig_len;
	void* captured;
};

static void close_output_fifo(struct handler_data* data) {
	if (data->out) {
		fclose(data->out);
		data->out = NULL;
	}
}

static void write_data(const void* ptr, size_t size, struct handler_data* data) {
	if ((data->out) && (fwrite(ptr, size, 1, data->out) != 1)) {
		close_output_fifo(data);
	}
}

static void write_uint16(uint16_t value, struct handler_data* data) {
	write_data(&value, sizeof(value), data);
}

static void write_uint32(uint32_t value, struct handler_data* data) {
	write_data(&value, sizeof(value), data);
}

static void flush_data(struct handler_data* data) {
	if ((data->out) && (fflush(data->out))) {
		close_output_fifo(data);
	}
}

static void* malloc_abort_on_failure(size_t size) {
	void* allocated = malloc(size);
	if (!allocated) {
		abort();
	}
	return allocated;
}

static void queue_packet(struct pcap_packet* pkt, struct handler_data* data) {
	const size_t record_length = (4 * sizeof(uint32_t)) + pkt->incl_len;
	uint8_t* buffer;
	record_list** ptr;

	ptr = &data->queue;
	while (*ptr) {
		ptr = &((*ptr)->next);
	}

	buffer = (uint8_t*)malloc_abort_on_failure(record_length);
	*ptr = (record_list*)malloc_abort_on_failure(sizeof(record_list));
	(*ptr)->next = NULL;
	(*ptr)->record = buffer;
	(*ptr)->record_length = record_length;

	memcpy(buffer, &pkt->ts_sec, sizeof(uint32_t));
	buffer += sizeof(uint32_t);
	memcpy(buffer, &pkt->ts_nsec, sizeof(uint32_t));
	buffer += sizeof(uint32_t);
	memcpy(buffer, &pkt->incl_len, sizeof(uint32_t));
	buffer += sizeof(uint32_t);
	memcpy(buffer, &pkt->orig_len, sizeof(uint32_t));
	buffer += sizeof(uint32_t);

	memcpy(buffer, pkt->captured, pkt->incl_len);
}

static void free_queued_packets(struct handler_data* data) {
	record_list* ptr = data->queue;
	while (ptr) {
		record_list* next = ptr->next;
		free(ptr->record);
		free(ptr);
		ptr = next;
	}
	data->queue = NULL;
}

static void forward_queued_packets(struct handler_data* data) {
	for (record_list* ptr = data->queue; ptr; ptr = ptr->next) {
		write_data(ptr->record, ptr->record_length, data);
		flush_data(data);
	}
	free_queued_packets(data);
}

static void discard_queued_packets(struct handler_data* data) {
	if (!data->filter_naks) {
		/* We are only running the state machine to filter SOFs, forward all packets */
		forward_queued_packets(data);
	} else {
		free_queued_packets(data);
	}
}

static void forward_packet(struct pcap_packet* packet, struct handler_data* data) {
	write_uint32(packet->ts_sec, data);
	write_uint32(packet->ts_nsec, data);
	write_uint32(packet->incl_len, data);
	write_uint32(packet->orig_len, data);

	write_data(packet->captured, packet->incl_len, data);
	flush_data(data);
}

static void discard_packet(struct pcap_packet* packet, struct handler_data* data) {
	if (!data->filter_naks) {
		/* We are only running the state machine to filter SOFs, forward the packet */
		forward_packet(packet, data);
	}
}

static void filter_packet(struct pcap_packet* packet, struct handler_data* data) {
	uint8_t PID;

	/* Queue must be empty when in default or split state */
	assert((data->st != FILTER_STATE_DEFAULT) || (!data->queue));
	assert((data->st != FILTER_STATE_SPLIT) || (!data->queue));
	assert((packet->incl_len > 0) && (packet->captured));

	PID = *((uint8_t*)packet->captured);

	if (PID == USB_PID_TOKEN_SOF) {
		if (data->st == FILTER_STATE_DEFAULT) {
			if (!data->filter_sofs) {
				forward_packet(packet, data);
			}
			/* else don't do anything with this filtered uninteresting SOF */
		} else {
			forward_queued_packets(data);
			forward_packet(packet, data);
			data->st = FILTER_STATE_DEFAULT;
		}
	} else if (PID == USB_PID_SPECIAL_SPLIT) {
		forward_queued_packets(data);
		forward_packet(packet, data);
		data->st = FILTER_STATE_SPLIT;
	} else if (data->st == FILTER_STATE_SPLIT) {
		/* TODO: Implement SPLIT transaction NAK filtering */
		forward_packet(packet, data);
		data->st = FILTER_STATE_DEFAULT;
	} else if (PID == USB_PID_TOKEN_OUT) {
		forward_queued_packets(data);
		queue_packet(packet, data);
		data->st = FILTER_STATE_OUT;
	} else if (data->st == FILTER_STATE_OUT && ((PID == USB_PID_DATA_DATA0) || (PID == USB_PID_DATA_DATA1))) {
		queue_packet(packet, data);
		data->st = FILTER_STATE_EXPECT_NAK;
	} else if ((PID == USB_PID_TOKEN_IN) || (PID == USB_PID_SPECIAL_PING)) {
		forward_queued_packets(data);
		queue_packet(packet, data);
		data->st = FILTER_STATE_EXPECT_NAK;
	} else if ((data->st == FILTER_STATE_EXPECT_NAK) && (PID == USB_PID_HANDSHAKE_NAK)) {
		discard_queued_packets(data);
		discard_packet(packet, data);
		data->st = FILTER_STATE_DEFAULT;
	} else {
		forward_queued_packets(data);
		forward_packet(packet, data);
		data->st = FILTER_STATE_DEFAULT;
	}
}

static void packet_handler(struct ov_packet* packet, void* user_data) {
	struct handler_data* data = (struct handler_data*)user_data;
	uint32_t nsec;
	/* Increment timestamp based on the 60 MHz 24-bit counter value.
	 * Convert remaining clocks to nanoseconds: 1 clk = 1 / 60 MHz = 16.(6) ns
	 */
	uint64_t clks;
	if (packet->timestamp < data->last_ov_ts) {
		data->ts_offset += (1 << 24);
	}
	data->last_ov_ts = packet->timestamp;
	clks = data->ts_offset + packet->timestamp;
	if (clks >= OV_TIMESTAMP_FREQ_HZ) {
		data->utc_ts += 1;
		data->ts_offset -= OV_TIMESTAMP_FREQ_HZ;
		clks -= OV_TIMESTAMP_FREQ_HZ;
	}
	nsec = (clks * 17) - (clks / 3);

	/* Only write actual USB packets */
	if (packet->size > 0) {
		struct pcap_packet pkt = {
		    .ts_sec = data->utc_ts,
		    .ts_nsec = nsec,
		    /* TODO: Modify OV firmware to provide info how long was truncated packet
		     * Currently this code lies that all packets are fully captured...
		     */
		    .incl_len = packet->size,
		    .orig_len = packet->size,
		    .captured = packet->data,
		};

		filter_packet(&pkt, data);
	}
}

static int start_capture(enum ov_usb_speed speed, bool filter_naks, bool filter_sofs, const char* extcap_fifo) {
	int ret;
	struct timespec ts;
	struct handler_data data;
	struct ov_device* ov;
	union {
		struct ov_packet packet;
		char buf[1024];
	} p;

	ov = ov_new(NULL);
	if (!ov) {
		fprintf(stderr, "Cannot create ov_device handler\n");
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

	data.out = fopen(extcap_fifo, "wb");
	if (!data.out) {
		fprintf(stderr, "Cannot open fifo for writing\n");

		ov_free(ov);
		return 1;
	}
	/* Assume OpenVizsla timestamp value 0 is the current realtime */
#ifdef _MSC_VER
	timespec_get(&ts, TIME_UTC);
#else
	clock_gettime(CLOCK_REALTIME, &ts);
#endif
	data.utc_ts = ts.tv_sec;
	data.last_ov_ts = 0;
	data.ts_offset = (ts.tv_nsec / 17) + (ts.tv_nsec / 850);

	data.filter_naks = filter_naks;
	data.filter_sofs = filter_sofs;
	data.st = FILTER_STATE_DEFAULT;
	data.queue = NULL;

	/* Write pcap header */
	write_uint32(PCAP_NANOSEC_MAGIC, &data);
	write_uint16(2, &data);
	write_uint16(4, &data);
	write_uint32(0, &data);
	write_uint32(0, &data);
	write_uint32(65535, &data);
	write_uint32(LINKTYPE_USBLL, &data);
	flush_data(&data);

	ret = ov_capture_start(ov, &p.packet, sizeof(p), &packet_handler, &data);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot start capture", ov_get_error_string(ov));

		close_output_fifo(&data);
		ov_free(ov);
		return 1;
	}

	ret = 0;
	/* Keep running until stopped in Wireshark (output fifo gets closed) */
	while (data.out) {
		ret = ov_capture_dispatch(ov, 1);
		if (ret == -1) {
			fprintf(stderr, "%s: %s\n", "Cannot dispatch capture", ov_get_error_string(ov));
			break;
		}
	}

	discard_queued_packets(&data);
	close_output_fifo(&data);
	ov_capture_stop(ov);
	ov_free(ov);
	return ret;
}

static void print_extcap_version(void) {
	printf("extcap {version=" EXTCAP_VERSION_STR "}{help=https://github.com/matwey/libopenvizsla/}\n");
}

static void print_extcap_interfaces(void) {
	printf("interface {value=ov}{display=OpenVizsla FPGA-based USB sniffer}\n");
}

static void print_extcap_dlts(void) {
	printf("dlt {number=%d}{name=USB_2_0}{display=USB 2.0/1.1/1.0}\n", LINKTYPE_USBLL);
}

static void print_extcap_options(void) {
	printf("arg {number=0}{call=--speed}"
	       "{display=Capture speed}{tooltip=Analyzed device USB speed}"
	       "{type=selector}{default=%d}\n",
	       OV_HIGH_SPEED);
	printf("value {arg=0}{value=%d}{display=High}\n", OV_HIGH_SPEED);
	printf("value {arg=0}{value=%d}{display=Full}\n", OV_FULL_SPEED);
	printf("value {arg=0}{value=%d}{display=Low}\n", OV_LOW_SPEED);
	printf("arg {number=1}{call=--filter-nak}"
	       "{display=Filter NAKed transactions}"
	       "{tooltip=NAKed SPLIT transactions won't be filtered}"
	       "{type=boolflag}{default=false}\n");
	printf("arg {number=2}{call=--filter-sof}"
	       "{display=Filter Full and High speed Start-of-Frame packets}"
	       "{tooltip=Only SOFs that do not interrupt any transaction will be filtered}"
	       "{type=boolflag}{default=false}\n");
}

int main(int argc, char** argv) {
	enum ov_usb_speed speed = OV_HIGH_SPEED;

	int filter_naks = 0;
	int filter_sofs = 0;

	int do_extcap_version = 0;
	int do_extcap_interfaces = 0;
	int do_extcap_dlts = 0;
	int do_extcap_config = 0;
	int do_extcap_capture = 0;
	const char* wireshark_version = NULL;
	const char* extcap_interface = NULL;
	const char* extcap_fifo = NULL;

#define ARG_EXTCAP_VERSION 1000
#define ARG_EXTCAP_INTERFACE 1001
#define ARG_EXTCAP_FIFO 1002

	struct option long_options[] = {{"speed", required_argument, 0, 's'},
	                                {"filter-nak", no_argument, &filter_naks, 1},
	                                {"filter-sof", no_argument, &filter_sofs, 1},
	                                /* Extcap interface. Please note that there are no short
	                                 * options for these and the numbers are just gopt keys.
	                                 */
	                                {"extcap-version", optional_argument, 0, ARG_EXTCAP_VERSION},
	                                {"extcap-interface", required_argument, 0, ARG_EXTCAP_INTERFACE},
	                                {"fifo", required_argument, 0, ARG_EXTCAP_FIFO},
	                                {"extcap-interfaces", no_argument, &do_extcap_interfaces, 1},
	                                {"extcap-dlts", no_argument, &do_extcap_dlts, 1},
	                                {"extcap-config", no_argument, &do_extcap_config, 1},
	                                {"capture", no_argument, &do_extcap_capture, 1},
	                                {0, 0, 0, 0}};
	int option_index = 0;
	int c;

	while (-1 != (c = getopt_long(argc, argv, "s:", long_options, &option_index))) {
		switch (c) {
			case 0:
				/* getopt_long has set the flag. */
				break;
			case 's': /* --speed */
				speed = atoi(optarg);
				switch (speed) {
					case OV_LOW_SPEED:
					case OV_FULL_SPEED:
					case OV_HIGH_SPEED:
						break;
					default:
						fprintf(stderr, "Invalid speed option!\n");
						return -1;
				}
				break;
			case ARG_EXTCAP_VERSION:
				do_extcap_version = 1;
				wireshark_version = optarg;
				break;
			case ARG_EXTCAP_INTERFACE:
				extcap_interface = optarg;
				break;
			case ARG_EXTCAP_FIFO:
				extcap_fifo = optarg;
				break;

			default:
				printf("getopt_long() returned character code 0x%X. Please report.\n", c);
				return -1;
		}
	}

	if (do_extcap_version) {
		print_extcap_version();
	}

	if (do_extcap_interfaces) {
		print_extcap_interfaces();
	}

	if (do_extcap_dlts) {
		print_extcap_dlts();
	}

	if (do_extcap_config) {
		print_extcap_options();
	}

	if (do_extcap_capture) {
		if ((!extcap_fifo) || (!extcap_interface)) {
			/* No fifo nor interface to capture from. */
			return -1;
		}

		return start_capture(speed, filter_naks, filter_sofs, extcap_fifo);
	}

	return 0;
}
