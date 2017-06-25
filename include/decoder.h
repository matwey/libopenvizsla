#ifndef _DECODER_H
#define _DECODER_H

#include <memory.h>
#include <stdint.h>

typedef void (*packet_decoder_callback)(uint8_t*, size_t, void*);

struct packet_decoder {
	uint8_t* buf;
	size_t   buf_actual_length;
	size_t   buf_length;
	char*    error_str;

	packet_decoder_callback callback;
	void*    user_data;

	enum packet_decoder_state {
		NEED_PACKET_MAGIC,
		NEED_PACKET_LENGTH,
		NEED_PACKET_DATA
	} state;
	size_t   required_length;
};

int packet_decoder_init(struct packet_decoder* pd, uint8_t* buf, size_t size, packet_decoder_callback callback, void* data);
int packet_decoder_proc(struct packet_decoder* pd, uint8_t* buf, size_t size);

struct frame_decoder {
	struct packet_decoder pd;

	char* error_str;

	enum frame_decoder_state {
		NEED_FRAME_MAGIC,
		NEED_FRAME_LENGTH,
		NEED_FRAME_DATA
	} state;
	size_t required_length;
};

int frame_decoder_init(struct frame_decoder* fd, uint8_t* buf, size_t size, packet_decoder_callback callback, void* data);
int frame_decoder_proc(struct frame_decoder* fd, uint8_t* buf, size_t size);

#endif // _DECODER_H
