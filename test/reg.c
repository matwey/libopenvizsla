#include <check.h>
#include <stdlib.h>

#include <reg.h>

START_TEST (test_reg_init1) {
	struct reg reg;
	ck_assert_int_eq(reg_init(&reg), 0);
}
END_TEST
START_TEST (test_reg_from_map1) {
	char x[] = "LEDS_OUT = 0x42";
	struct reg reg;
	ck_assert_int_eq(reg_init(&reg), 0);
	ck_assert_int_eq(reg_from_map(&reg, x), 0);
	ck_assert_int_eq(reg.addr[LEDS_OUT], 0x42);
}
END_TEST

Suite* range_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("reg");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_reg_init1);
	tcase_add_test(tc_core, test_reg_from_map1);
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

