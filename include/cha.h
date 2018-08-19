#ifndef _CHA_H
#define _CHA_H

#include <ftdi.h>
#include <decoder.h>

#include <stdint.h>
#include <memory.h>

struct cha {
	struct ftdi_context ftdi;
	const char* error_str;
};

int cha_init(struct cha* cha);
int cha_open(struct cha* cha);
int cha_switch_config_mode(struct cha* cha);
int cha_switch_fifo_mode(struct cha* cha);
int cha_write_reg(struct cha* cha, uint16_t addr, uint8_t val);
int cha_read_reg(struct cha* cha, uint16_t addr, uint8_t* val);
int cha_write_reg32(struct cha* cha, uint16_t addr, uint32_t val);
int cha_read_reg32(struct cha* cha, uint16_t addr, uint32_t* val);
int cha_start_stream(struct cha* cha);
int cha_stop_stream(struct cha* cha);
int cha_loop(struct cha* cha, size_t count, packet_decoder_callback callback, void* data);
void cha_destroy(struct cha* cha);

const char* cha_get_error_string(struct cha* cha);

#endif // _CHA_H
