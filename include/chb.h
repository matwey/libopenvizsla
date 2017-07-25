#ifndef _CHB_H
#define _CHB_H

#include <ftdi.h>

#include <stdint.h>

struct chb {
	struct ftdi_context ftdi;
	const char* error_str;
};

int chb_init(struct chb* chb);
int chb_open(struct chb* chb);
int chb_set_low(struct chb* chb, uint8_t val);
int chb_set_high(struct chb* chb, uint8_t val);
int chb_get_low(struct chb* chb, uint8_t* val);
int chb_get_high(struct chb* chb, uint8_t* val);
int chb_get_status(struct chb* chb, uint8_t* status);
int chb_switch_program_mode(struct chb* chb);
void chb_destroy(struct chb* chb);

const char* chb_get_error_string(struct chb* chb);

#endif // _CHB_H
