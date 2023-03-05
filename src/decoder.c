/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <decoder.h>

#include <assert.h>

int packet_decoder_init(struct packet_decoder* pd, struct ov_packet* p, size_t size, ov_packet_decoder_callback callback, void* data) {
	pd->packet = p;
	pd->buf_actual_length = 0;
	pd->buf_length = size;
	pd->callback = callback;
	pd->user_data = data;
	pd->error_str = NULL;
	pd->state = NEED_PACKET_MAGIC;

	return 0;
}

int packet_decoder_proc(struct packet_decoder* pd, uint8_t* buf, size_t size) {
	const uint8_t* end = buf + size;

	while (buf != end) {
		switch (pd->state) {
			case NEED_PACKET_MAGIC: {
				assert(pd->buf_actual_length == 0);
				assert(pd->buf_length > 0);

				if (*buf != 0xa0) {
					pd->error_str = "Wrong packet magic";
					return -1;
				}

				pd->packet->magic = *(buf++);
				pd->state = NEED_PACKET_FLAGS_LO;
			} break;
			case NEED_PACKET_FLAGS_LO: {
				pd->packet->flags = *(buf++);
				pd->state = NEED_PACKET_FLAGS_HI;
			} break;
			case NEED_PACKET_FLAGS_HI: {
				pd->packet->flags = (*(buf++) << 8) | pd->packet->flags;
				pd->state = NEED_PACKET_LENGTH_LO;
			} break;
			case NEED_PACKET_LENGTH_LO: {
				pd->packet->size = *(buf++);
				pd->state = NEED_PACKET_LENGTH_HI;
			} break;
			case NEED_PACKET_LENGTH_HI: {
				pd->packet->size = (*(buf++) << 8) | pd->packet->size;
				pd->state = NEED_PACKET_TIMESTAMP_LO;

				// FIXME: check available buffer size
			} break;
			case NEED_PACKET_TIMESTAMP_LO: {
				pd->packet->timestamp = *(buf++);
				pd->state = NEED_PACKET_TIMESTAMP_ME;
			} break;
			case NEED_PACKET_TIMESTAMP_ME: {
				pd->packet->timestamp |= *(buf++) << 8;
				pd->state = NEED_PACKET_TIMESTAMP_HI;
			} break;
			case NEED_PACKET_TIMESTAMP_HI: {
				pd->packet->timestamp = (*(buf++) << 16) | pd->packet->timestamp;
				pd->state = NEED_PACKET_DATA;
			} break;
			case NEED_PACKET_DATA: {
				const size_t required_length = pd->packet->size - pd->buf_actual_length;
				const size_t copy = (required_length < (end - buf) ? required_length : end - buf);

				memcpy(pd->packet->data + pd->buf_actual_length, buf, copy);
				pd->buf_actual_length += copy;
				buf += copy;

				if (required_length == copy) {
					/* Finalize packet here */
					if (pd->callback) {
						pd->callback(pd->packet, pd->user_data);
					}

					pd->buf_actual_length = 0;
					pd->state = NEED_PACKET_MAGIC;

					goto end;
				}
			} break;
		}
	}

end:
	return size - (end - buf);
}

ov_packet_decoder_callback packet_decoder_set_callback(struct packet_decoder* pd, ov_packet_decoder_callback callback, void* user_data) {
	ov_packet_decoder_callback old_callback = pd->callback;

	pd->callback = callback;
	pd->user_data = user_data;

	return old_callback;
}

int frame_decoder_init(struct frame_decoder* fd, struct ov_packet* p, size_t size, ov_packet_decoder_callback callback, void* data) {
	if (packet_decoder_init(&fd->pd, p, size, callback, data) < 0)
		return -1;

	fd->error_str = NULL;
	fd->state = NEED_FRAME_MAGIC;
	fd->required_length = 0;

	return 0;
}

int frame_decoder_proc(struct frame_decoder* fd, uint8_t* buf, size_t size) {
	const uint8_t* end = buf + size;

	while (buf != end) {
		switch (fd->state) {
			case NEED_FRAME_MAGIC: {
				if (*buf++ != 0xd0) {
					fd->error_str = "Wrong frame magic";
					return -1;
				}

				fd->state = NEED_FRAME_LENGTH;
			} break;
			case NEED_FRAME_LENGTH: {
				fd->required_length = ((size_t)(*buf++)+1)*2;
				fd->state = NEED_FRAME_DATA;
			} break;
			case NEED_FRAME_DATA: {
				const size_t psize = (fd->required_length < end - buf ? fd->required_length : end - buf);
				int ret = 0;

				ret = packet_decoder_proc(&fd->pd, buf, psize);
				if (ret == -1) {
					fd->error_str = fd->pd.error_str;
					return -1;
				}

				buf += ret;
				fd->required_length -= ret;

				if (fd->required_length == 0) {
					fd->state = NEED_FRAME_MAGIC;
				}
			} break;
		}
	}

	return size - (end - buf);
}

ov_packet_decoder_callback frame_decoder_set_callback(struct frame_decoder* fd, ov_packet_decoder_callback callback, void* user_data) {
	return packet_decoder_set_callback(&fd->pd, callback, user_data);
}
