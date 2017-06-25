#include <decoder.h>

#include <assert.h>

int packet_decoder_init(struct packet_decoder* pd, uint8_t* buf, size_t size, packet_decoder_callback callback, void* data) {
	pd->buf = buf;
	pd->buf_actual_length = 0;
	pd->buf_length = size;
	pd->callback = callback;
	pd->user_data = data;
	pd->error_str = NULL;
	pd->state = NEED_PACKET_MAGIC;
	pd->required_length = 0;

	return 0;
}

int packet_decoder_proc(struct packet_decoder* pd, uint8_t* buf, size_t size) {
	const uint8_t* end = buf + size;

	while (buf != end) {
		switch (pd->state) {
			case NEED_PACKET_MAGIC: {
				assert(pd->buf_actual_length == 0);
				assert(pd->buf_length > 0);

				if (buf[0] != 0xa0) {
					pd->error_str = "Wrong packet magic";
					return -1;
				}

				pd->buf[pd->buf_actual_length++] = *(buf++);
				pd->state = NEED_PACKET_LENGTH;
			} break;
			case NEED_PACKET_LENGTH: {
				assert(pd->buf_actual_length >= 1);
				assert(pd->buf_actual_length < 5);
				assert(pd->buf_length > 5);

				for (; pd->buf_actual_length < 5 && buf != end; ++buf)
					pd->buf[pd->buf_actual_length++] = *buf;

				if (pd->buf_actual_length == 5) {
					pd->required_length = ((((size_t)(pd->buf[4])) << 8) | pd->buf[3]) + 8 - pd->buf_actual_length;
					pd->state = NEED_PACKET_DATA;
				}
			} break;
			case NEED_PACKET_DATA: {
				const size_t copy = (pd->required_length < (end - buf) ? pd->required_length : end - buf);
				const size_t copy2 = (copy + pd->buf_actual_length > pd->buf_length ? pd->buf_length - pd->buf_actual_length : copy);

				assert(pd->buf_actual_length >= 5);

				memcpy(pd->buf + pd->buf_actual_length, buf, copy2);
				pd->buf_actual_length += copy2;
				buf += copy;
				pd->required_length -= copy;

				if (pd->required_length == 0) {
					/* Finalize packet here*/
					pd->callback(pd->buf, pd->buf_actual_length, pd->user_data);

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

int frame_decoder_init(struct frame_decoder* fd, uint8_t* buf, size_t size, packet_decoder_callback callback, void* data) {
	if (packet_decoder_init(&fd->pd, buf, size, callback, data) != 0)
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
				const size_t size = (fd->required_length < end - buf ? fd->required_length : end - buf);
				int ret = 0;

				ret = packet_decoder_proc(&fd->pd, buf, size);
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
