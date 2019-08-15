/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef _REG_H
#define _REG_H

#include <fwpkg.h>

#include <stdint.h>

enum reg_name {
	CSTREAM_CFG,
	CSTREAM_CONS_LO,
	CSTREAM_CONS_HI,

	LEDS_OUT,

	SDRAM_HOST_READ_GO,

	SDRAM_SINK_GO,

	UCFG_WDATA,
	UCFG_WCMD,
	UCFG_RDATA,
	UCFG_RCMD,

	REG_MAX
};

struct reg {
	const char* error_str;
	uint16_t addr[REG_MAX];
};

int reg_init(struct reg* reg, char* map);
int reg_init_from_fwpkg(struct reg* reg, struct fwpkg* fwpkg);
int reg_init_from_reg(struct reg* reg, struct reg* other);

const char* reg_get_error_string(struct reg* reg);

#endif // _REG_H
