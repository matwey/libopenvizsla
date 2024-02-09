/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <decoder.h>

#include <assert.h>


#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int packet_decoder_init(struct packet_decoder* pd, struct ov_packet* p, size_t size, const struct decoder_ops* ops, void* user_data) {
	pd->packet = p;
	pd->ops = *ops;
	pd->user_data = user_data;
	pd->state = NEED_PACKET_MAGIC;
	pd->buf_actual_length = 0;
	pd->buf_length = size;
	pd->error_str = NULL;
	pd->cumulative_ts = 0;
	pd->ts_byte = 0;
	pd->ts_length = 0;
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

				if (*buf == 0xa1) {
					/* Discard magic filler */
					buf++;
					break;
				}
				if ((*buf != 0xa0) && (*buf != 0xa2)) {
					pd->error_str = "Wrong packet magic";
					return -1;
				}

				pd->packet->magic = *(buf++);
				pd->state = NEED_PACKET_FLAGS;
			} break;
			case NEED_PACKET_FLAGS: {
				pd->packet->flags = *(buf++);
				pd->state = NEED_PACKET_LENGTH_LO;
			} break;
			case NEED_PACKET_LENGTH_LO: {
				pd->packet->size = *(buf++);
				pd->state = NEED_PACKET_LENGTH_HI;
			} break;
			case NEED_PACKET_LENGTH_HI: {
				pd->packet->size = ((*buf & 0x1f) << 8) | pd->packet->size;
				pd->ts_length = (((*(buf++)) & 0xe0) >> 5) + 1;
				pd->ts_byte = 0;
				pd->packet->timestamp = 0;
				pd->state = NEED_PACKET_TIMESTAMP;

				// FIXME: check available buffer size
			} break;
			case NEED_PACKET_TIMESTAMP: {
				pd->packet->timestamp |= ((uint64_t)*(buf++)) << (8 * (pd->ts_byte++));
				if (pd->ts_byte >= pd->ts_length) {
					pd->cumulative_ts += pd->packet->timestamp;
					pd->packet->timestamp = pd->cumulative_ts;
					pd->state = NEED_PACKET_DATA;
				}
			} break;
			case NEED_PACKET_DATA: {
				const size_t required_length = ov_packet_captured_size(pd->packet) - pd->buf_actual_length;
				const size_t copy = MIN(required_length, end - buf);

				memcpy(pd->packet->data + pd->buf_actual_length, buf, copy);
				pd->buf_actual_length += copy;
				buf += copy;

				if (required_length == copy) {
					/* Finalize packet here */
					if (pd->ops.packet) {
						pd->ops.packet(pd->user_data, pd->packet);
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

int frame_decoder_init(struct frame_decoder* fd, struct ov_packet* p, size_t size, const struct decoder_ops* ops, void* user_data) {
	if (packet_decoder_init(&fd->pd, p, size, ops, user_data) < 0)
		return -1;

	fd->state = NEED_FRAME_MAGIC;

	return 0;
}

int frame_decoder_proc(struct frame_decoder* fd, uint8_t* buf, size_t size) {
	const uint8_t* end = buf + size;

	while (buf != end) {
		switch (fd->state) {
			case NEED_FRAME_MAGIC: switch (*buf++) {
				case 0x55: {
					fd->state = NEED_BUS_FRAME_ADDR_HI;
				} break;
				case 0xd0: {
					fd->state = NEED_SDRAM_FRAME_LENGTH;
				} break;
				default: {
					fd->pd.error_str = "Wrong frame magic";
					return -1;
				} break;
			}; break;
			case NEED_SDRAM_FRAME_LENGTH: {
				fd->sdram.required_length = ((uint16_t)(*buf++)+1)*2;
				fd->state = NEED_SDRAM_FRAME_DATA;
			} break;
			case NEED_SDRAM_FRAME_DATA: {
				const size_t psize = MIN(fd->sdram.required_length, end - buf);
				int ret = 0;

				ret = packet_decoder_proc(&fd->pd, buf, psize);
				if (ret < 0) {
					return ret;
				}

				buf += ret;
				fd->sdram.required_length -= ret;

				if (fd->sdram.required_length == 0) {
					fd->state = NEED_FRAME_MAGIC;
				}
			} break;
			case NEED_BUS_FRAME_ADDR_HI: {
				fd->bus.addr = ((uint16_t)(*buf++)) << 8;
				fd->state = NEED_BUS_FRAME_ADDR_LO;
			} break;
			case NEED_BUS_FRAME_ADDR_LO: {
				fd->bus.addr |= *buf++;
				fd->state = NEED_BUS_FRAME_VALUE;
			} break;
			case NEED_BUS_FRAME_VALUE: {
				fd->bus.value = *buf++;
				fd->state = NEED_BUS_FRAME_CHECKSUM;
			} break;
			case NEED_BUS_FRAME_CHECKSUM: {
				fd->bus.checksum = *buf++;
				fd->state = NEED_FRAME_MAGIC;

				if (fd->pd.ops.bus_frame) {
					fd->pd.ops.bus_frame(fd->pd.user_data, fd->bus.addr, fd->bus.value);
				}
			} break;
		}
	}

	return size - (end - buf);
}
