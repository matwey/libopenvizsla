/*
 * Copyright (c) 2019 Tomasz Moń <desowin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 199309L
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

#include <openvizsla.h>

#define EXTCAP_VERSION_STR "0.0.3"

#define EXTCAP_INTERFACE_DEPRECATED "ov"
#define EXTCAP_INTERFACE_LOW_SPEED "ov-low"
#define EXTCAP_INTERFACE_FULL_SPEED "ov-full"
#define EXTCAP_INTERFACE_HIGH_SPEED "ov-high"

enum wireshark_version {
	WIRESHARK_VERSION_UNKNOWN,
	WIRESHARK_VERSION_2_9_OR_NEWER, /* Supports --extcap-version */
	WIRESHARK_VERSION_3_1_OR_NEWER, /* Supports obsolete USBLL linklayer type */
	WIRESHARK_VERSION_4_0_OR_NEWER, /* Supports speed specific linklayer types */
};

#define PCAP_NANOSEC_MAGIC (0xa1b23c4d)
#define LINKTYPE_USBLL (288)
#define LINKTYPE_USBLL_LOW_SPEED (293)
#define LINKTYPE_USBLL_FULL_SPEED (294)
#define LINKTYPE_USBLL_HIGH_SPEED (295)

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
	struct ov_device *ov;/**< Capture device */
	FILE* out;           /**< Output file. Set to NULL if capture stopped from Wireshark (broken pipe). */
	FILE* debug;         /**< Debug output file. NULL if not writing to debug file. */
	uint32_t utc_ts;     /**< Seconds since epoch */
	uint32_t ts_offset;  /**< Timestamp offset in 1/OV_TIMESTAMP_FREQ_HZ units */
	uint64_t last_ov_ts; /**< Last received packet timestamp */

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

static void close_file(FILE** out) {
	if (*out) {
		fclose(*out);
		*out = NULL;
	}
}

static void write_data(const void* ptr, size_t size, FILE** out) {
	if ((*out) && (fwrite(ptr, size, 1, *out) != 1)) {
		close_file(out);
	}
}

static void write_uint16(uint16_t value, FILE** out) {
	write_data(&value, sizeof(value), out);
}

static void write_uint32(uint32_t value, FILE** out) {
	write_data(&value, sizeof(value), out);
}

static void flush_data(FILE** out) {
	if ((*out) && (fflush(*out))) {
		close_file(out);
	}
}

static void write_pcap_header(FILE** out, uint32_t linktype) {
	write_uint32(PCAP_NANOSEC_MAGIC, out);
	write_uint16(2, out);
	write_uint16(4, out);
	write_uint32(0, out);
	write_uint32(0, out);
	write_uint32(65535, out);
	write_uint32(linktype, out);
	flush_data(out);
}

static void write_packet(struct pcap_packet* packet, FILE** out) {
	write_uint32(packet->ts_sec, out);
	write_uint32(packet->ts_nsec, out);
	write_uint32(packet->incl_len, out);
	write_uint32(packet->orig_len, out);

	write_data(packet->captured, packet->incl_len, out);
	flush_data(out);
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
		write_data(ptr->record, ptr->record_length, &data->out);
		flush_data(&data->out);
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
	write_packet(packet, &data->out);
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
	uint64_t diff_ts;
	diff_ts = packet->timestamp - data->last_ov_ts;
	data->last_ov_ts = packet->timestamp;
	data->utc_ts += (data->ts_offset + diff_ts) / OV_TIMESTAMP_FREQ_HZ;
	data->ts_offset = (data->ts_offset + diff_ts) % OV_TIMESTAMP_FREQ_HZ;
	nsec = (data->ts_offset * 17) - (data->ts_offset / 3);

	/* Only write actual USB packets */
	if (packet->size > 0) {
		struct pcap_packet pkt = {
		    .ts_sec = data->utc_ts,
		    .ts_nsec = nsec,
		    .incl_len = ov_packet_captured_size(packet),
		    .orig_len = packet->size,
		    .captured = packet->data,
		};

		write_packet(&pkt, &data->debug);
		filter_packet(&pkt, data);

		/* Break out of the capture loop if output pipe breaks */
		if (!data->out) {
			ov_capture_breakloop(data->ov);
		}
	}
}

static int start_capture(enum ov_usb_speed speed, uint32_t linktype, bool filter_naks, bool filter_sofs, const char* extcap_fifo,
                         FILE* debug_pcap) {
	int ret;
	struct timespec ts;
	struct handler_data data;
	union {
		struct ov_packet packet;
		char buf[1024];
	} p;

	data.ov = ov_new(NULL);
	if (!data.ov) {
		fprintf(stderr, "Cannot create ov_device handler\n");
		return 1;
	}

	ret = ov_open(data.ov);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot open OpenVizsla device", ov_get_error_string(data.ov));

		ov_free(data.ov);
		return 1;
	}

	ret = ov_set_usb_speed(data.ov, speed);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot set USB speed", ov_get_error_string(data.ov));

		ov_free(data.ov);
		return 1;
	}

	data.out = fopen(extcap_fifo, "wb");
	if (!data.out) {
		fprintf(stderr, "Cannot open fifo for writing\n");

		ov_free(data.ov);
		return 1;
	}
	data.debug = debug_pcap;

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
	write_pcap_header(&data.out, linktype);
	write_pcap_header(&data.debug, linktype);

	ret = ov_capture_start(data.ov, &p.packet, sizeof(p), &packet_handler, &data);
	if (ret < 0) {
		fprintf(stderr, "%s: %s\n", "Cannot start capture", ov_get_error_string(data.ov));

		close_file(&data.out);
		close_file(&data.debug);
		ov_free(data.ov);
		return 1;
	}

	ret = ov_capture_dispatch(data.ov, 0);
	if (ret == -1) {
		fprintf(stderr, "%s: %s\n", "Cannot dispatch capture", ov_get_error_string(data.ov));
	}

	discard_queued_packets(&data);
	close_file(&data.out);
	close_file(&data.debug);
	ov_capture_stop(data.ov);
	ov_free(data.ov);
	return ret;
}

static enum wireshark_version wireshark_version_from_string(const char* version) {
	int major, minor;
	if (version && sscanf(version, "%d.%d", &major, &minor) == 2) {
		if (major >= 4) {
			return WIRESHARK_VERSION_4_0_OR_NEWER;
		} else if ((major == 3) && (minor >= 1)) {
			return WIRESHARK_VERSION_3_1_OR_NEWER;
		}
		return WIRESHARK_VERSION_2_9_OR_NEWER;
	}
	return WIRESHARK_VERSION_UNKNOWN;
}

static void print_extcap_version(void) {
	printf("extcap {version=" EXTCAP_VERSION_STR "}{help=https://github.com/matwey/libopenvizsla/}\n");
}

static void print_extcap_interfaces(enum wireshark_version ws_version) {
	if (ws_version == WIRESHARK_VERSION_3_1_OR_NEWER) {
		/* Wireshark has USBLL dissector but does not support speed specific linktypes */
		printf("interface {value=" EXTCAP_INTERFACE_DEPRECATED "}{display=OpenVizsla FPGA-based USB sniffer}\n");
	} else {
		printf("interface {value=" EXTCAP_INTERFACE_LOW_SPEED "}{display=OpenVizsla Low Speed USB capture}\n");
		printf("interface {value=" EXTCAP_INTERFACE_FULL_SPEED "}{display=OpenVizsla Full Speed USB capture}\n");
		printf("interface {value=" EXTCAP_INTERFACE_HIGH_SPEED "}{display=OpenVizsla High Speed USB capture}\n");
	}
}

static void print_extcap_dlts(const char* interface) {
	if (!strcmp(interface, EXTCAP_INTERFACE_DEPRECATED)) {
		printf("dlt {number=%d}{name=USB_2_0}{display=USB 2.0/1.1/1.0}\n", LINKTYPE_USBLL);
	} else if (!strcmp(interface, EXTCAP_INTERFACE_LOW_SPEED)) {
		printf("dlt {number=%d}{name=USB_2_0_LOW_SPEED}{display=Low-Speed USB 2.0/1.1/1.0}\n", LINKTYPE_USBLL_LOW_SPEED);
	} else if (!strcmp(interface, EXTCAP_INTERFACE_FULL_SPEED)) {
		printf("dlt {number=%d}{name=USB_2_0_FULL_SPEED}{display=Full-Speed USB 2.0/1.1/1.0}\n", LINKTYPE_USBLL_FULL_SPEED);
	} else if (!strcmp(interface, EXTCAP_INTERFACE_HIGH_SPEED)) {
		printf("dlt {number=%d}{name=USB_2_0_HIGH_SPEED}{display=High-Speed USB 2.0}\n", LINKTYPE_USBLL_HIGH_SPEED);
	}
}

static void print_extcap_options(const char* interface) {
	if (!strcmp(interface, EXTCAP_INTERFACE_DEPRECATED)) {
		printf("arg {number=0}{call=--speed}"
		       "{display=Capture speed}{tooltip=Analyzed device USB speed}"
		       "{type=selector}{default=%d}{group=Capture}\n",
		       OV_HIGH_SPEED);
		printf("value {arg=0}{value=%d}{display=High}\n", OV_HIGH_SPEED);
		printf("value {arg=0}{value=%d}{display=Full}\n", OV_FULL_SPEED);
		printf("value {arg=0}{value=%d}{display=Low}\n", OV_LOW_SPEED);
	}

	printf("arg {number=1}{call=--filter-nak}"
	       "{display=Filter NAKed transactions}"
	       "{tooltip=NAKed SPLIT transactions won't be filtered}"
	       "{type=boolflag}{default=false}{group=Capture}\n");

	if (!strcmp(interface, EXTCAP_INTERFACE_DEPRECATED)) {
		printf("arg {number=2}{call=--filter-sof}"
		       "{display=Filter Full and High speed Start-of-Frame packets}"
		       "{tooltip=Only SOFs that do not interrupt any transaction will be filtered}"
		       "{type=boolflag}{default=false}{group=Capture}\n");
	} else if (!strcmp(interface, EXTCAP_INTERFACE_FULL_SPEED) || !strcmp(interface, EXTCAP_INTERFACE_HIGH_SPEED)) {
		printf("arg {number=2}{call=--filter-sof}"
		       "{display=Filter Start-of-Frame packets}"
		       "{tooltip=Only SOFs that do not interrupt any transaction will be filtered}"
		       "{type=boolflag}{default=false}{group=Capture}\n");
	}

	printf("arg {number=800}{call=--overwrite-debug-pcap}{display=Overwrite .pcap file if it exists}{type=boolflag}"
	       "{required=false}{default=false}{group=Debug}\n");
	printf("arg {number=801}{call=--debug-pcap}{display=Save all packets to .pcap file}{type=fileselect}{fileext=pcap (*.pcap)}"
	       "{tooltip=Set a file where all incoming packets are written (unfiltered)}"
	       "{required=false}{group=Debug}\n");
}

int main(int argc, char** argv) {
	uint32_t linktype = LINKTYPE_USBLL;
	enum ov_usb_speed speed = OV_HIGH_SPEED;

	int filter_naks = 0;
	int filter_sofs = 0;
	int overwrite_debug_pcap = 0;
	int use_deprecated_linktype = 0;
	const char* debug_pcap_filename = NULL;
	FILE* debug_pcap = NULL;

	int do_extcap_version = 0;
	int do_extcap_interfaces = 0;
	int do_extcap_dlts = 0;
	int do_extcap_config = 0;
	int do_extcap_capture = 0;
	enum wireshark_version ws_version = WIRESHARK_VERSION_UNKNOWN;
	const char* extcap_interface = NULL;
	const char* extcap_fifo = NULL;

#define ARG_DEBUG_PCAP 801
#define ARG_EXTCAP_VERSION 1000
#define ARG_EXTCAP_INTERFACE 1001
#define ARG_EXTCAP_FIFO 1002

	struct option long_options[] = {{"speed", required_argument, 0, 's'},
	                                {"filter-nak", no_argument, &filter_naks, 1},
	                                {"filter-sof", no_argument, &filter_sofs, 1},
	                                {"overwrite-debug-pcap", no_argument, &overwrite_debug_pcap, 1},
	                                {"debug-pcap", required_argument, 0, ARG_DEBUG_PCAP},
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
			case ARG_DEBUG_PCAP:
				debug_pcap_filename = optarg;
				break;
			case ARG_EXTCAP_VERSION:
				do_extcap_version = 1;
				ws_version = wireshark_version_from_string(optarg);
				break;
			case ARG_EXTCAP_INTERFACE:
				extcap_interface = optarg;
				break;
			case ARG_EXTCAP_FIFO:
				extcap_fifo = optarg;
				break;

			default:
				fprintf(stderr, "getopt_long() returned character code 0x%X. Please report.\n", c);
				return -1;
		}
	}

	if (do_extcap_version) {
		print_extcap_version();
	}

	if (do_extcap_interfaces) {
		print_extcap_interfaces(ws_version);
	}

	if (extcap_interface) {
		if (do_extcap_dlts) {
			print_extcap_dlts(extcap_interface);
		}

		if (do_extcap_config) {
			print_extcap_options(extcap_interface);
		}

		if (!strcmp(extcap_interface, EXTCAP_INTERFACE_LOW_SPEED)) {
			linktype = LINKTYPE_USBLL_LOW_SPEED;
			speed = OV_LOW_SPEED;
		} else if (!strcmp(extcap_interface, EXTCAP_INTERFACE_FULL_SPEED)) {
			linktype = LINKTYPE_USBLL_FULL_SPEED;
			speed = OV_FULL_SPEED;
		} else if (!strcmp(extcap_interface, EXTCAP_INTERFACE_HIGH_SPEED)) {
			linktype = LINKTYPE_USBLL_HIGH_SPEED;
			speed = OV_HIGH_SPEED;
		}
	}

	if (do_extcap_capture) {
		if ((!extcap_fifo) || (!extcap_interface)) {
			/* No fifo nor interface to capture from. */
			return -1;
		}

		if (debug_pcap_filename) {
			if (overwrite_debug_pcap) {
				debug_pcap = fopen(debug_pcap_filename, "wb");
			} else {
				int fd = open(debug_pcap_filename,
#ifdef WIN32
				              O_CREAT | O_EXCL | O_WRONLY | O_BINARY, S_IREAD | S_IWRITE);
#else
				              O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
				if (-1 != fd) {
					debug_pcap = fdopen(fd, "wb");
				}
			}
			if (!debug_pcap) {
				fprintf(stderr, "Cannot create debug pcap file!\n");
				return -1;
			}
		}
		return start_capture(speed, linktype, filter_naks, filter_sofs, extcap_fifo, debug_pcap);
	}

	return 0;
}
