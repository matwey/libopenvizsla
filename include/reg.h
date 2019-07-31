/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _REG_H
#define _REG_H

#include <stdint.h>

enum reg_name {
	CSTREAM_CFG,
	CSTREAM_CONS_LO,
	CSTREAM_CONS_HI,

	LEDS_OUT,

	REG_MAX
};

struct reg {
	const char* error_str;
	uint16_t addr[REG_MAX];
};

int reg_init(struct reg* reg);
int reg_from_map(struct reg* reg, char* map);
int reg_validate(struct reg* reg);

#endif // _REG_H
