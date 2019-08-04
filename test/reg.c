#include <check.h>
#include <stdlib.h>

#include <fwpkg.h>
#include <reg.h>

struct fwpkg fwpkg;
char* map = NULL;

void setup() {
	int ret = 0;
	size_t map_size = 0;

	ret = fwpkg_init_from_preload(&fwpkg);
	ck_assert_int_eq(ret, 0);

	map_size = fwpkg_map_size(&fwpkg)+1;
	map = malloc(map_size);
	ck_assert(map != NULL);

	ret = fwpkg_read_map(&fwpkg, map, &map_size);
	ck_assert_int_eq(ret, 0);
	map[map_size] = '\0';
}

void teardown() {
	free(map);
	fwpkg_destroy(&fwpkg);
}

START_TEST (test_reg_from_map1) {
	char x[] = "LEDS_OUT = 0x42";
	struct reg reg;
	ck_assert_int_eq(reg_init(&reg, x), -1);
}
END_TEST

START_TEST (test_reg_from_map2) {
	struct reg reg;
	ck_assert_int_eq(reg_init(&reg, map), 0);
	ck_assert_int_eq(reg.addr[CSTREAM_CFG], 0x800);
	ck_assert_int_eq(reg.addr[CSTREAM_CONS_LO], 0x801);
	ck_assert_int_eq(reg.addr[CSTREAM_CONS_HI], 0x802);
}
END_TEST

Suite* range_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("reg");

	tc_core = tcase_create("Core");

	tcase_add_unchecked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, test_reg_from_map1);
	tcase_add_test(tc_core, test_reg_from_map2);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = range_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? 0 : 1;
}

