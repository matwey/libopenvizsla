#include <reg.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static char* x_strchr(const char *s, int c) {
	char* n = NULL;

	if ((n = strchr(s, c))) {
		*(n++) = '\0';
		return n;
	}

	return n;
}

static int reg_from_pair(struct reg* reg, char* key, unsigned long int value) {
	
	assert(value > 0);
	assert(value <= 0xFFFF);

	reg->leds_out = value;

	return 0;
}

static int reg_from_line(struct reg* reg, char* line) {
	char* key = line;
	char* value = NULL;

	if (line[0] == '\0' || line[0] == '#')
		return 0;

	if (!(value = strchr(line, '=')))
		return -1;
	*(value++) = '\0';

	return reg_from_pair(reg, key, strtoul(value, NULL, 16));
}

int reg_init(struct reg* reg) {
	memset(reg, 0, sizeof(struct reg));

	return 0;
}

int reg_from_map(struct reg* reg, char* map) {
	char* c = map;
	char* n = NULL;

	do {
		n = x_strchr(c, '\n');
		if (reg_from_line(reg, c)) {
			reg->error_str = "Can not parse register map";

			return -1;
		}
		c = n;
	} while (n);

	return 0;
}

int reg_validate(struct reg* reg) {

	return 0;
}
