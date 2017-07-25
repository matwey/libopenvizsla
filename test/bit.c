#include <check.h>
#include <stdlib.h>

#include <bit.h>

START_TEST (test_bit_init1) {
	static const uint8_t data[] = {
0x00, 0x09, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00, 0x01, 0x61, 0x00, 0x0a,
0x78, 0x66, 0x6f, 0x72, 0x6d, 0x2e, 0x6e, 0x63, 0x64, 0x00, 0x62, 0x00, 0x0c, 0x76, 0x31, 0x30,
0x30, 0x30, 0x65, 0x66, 0x67, 0x38, 0x36, 0x30, 0x00, 0x63, 0x00, 0x0b, 0x32, 0x30, 0x30, 0x31,
0x2f, 0x30, 0x38, 0x2f, 0x31, 0x30, 0x00, 0x64, 0x00, 0x09, 0x30, 0x36, 0x3a, 0x35, 0x35, 0x3a,
0x30, 0x34, 0x00, 0x65, 0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xff};

	struct bit bit;
	ck_assert_int_eq(bit_init(&bit, data, sizeof(data)), 0);
	ck_assert_str_eq(bit.ncd_filename, "xform.ncd");
	ck_assert_str_eq(bit.part_name, "v1000efg860");
	ck_assert_str_eq(bit.date, "2001/08/10");
	ck_assert_str_eq(bit.time, "06:55:04");
	ck_assert_uint_eq(bit.bit_length, 4);
}
END_TEST

START_TEST (test_bit_init2) {
	static const uint8_t data[] = {
0x00, 0x09, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00, 0x01, 0x61, 0x00, 0x1a,
0x6f, 0x76, 0x33, 0x2e, 0x6e, 0x63, 0x64, 0x3b, 0x55, 0x73, 0x65, 0x72, 0x49, 0x44, 0x3d, 0x30,
0x78, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x00, 0x62, 0x00, 0x0c, 0x36, 0x73, 0x6c,
0x78, 0x39, 0x74, 0x71, 0x67, 0x31, 0x34, 0x34, 0x00, 0x63, 0x00, 0x0b, 0x32, 0x30, 0x31, 0x34,
0x2f, 0x31, 0x31, 0x2f, 0x31, 0x30, 0x00, 0x64, 0x00, 0x09, 0x31, 0x36, 0x3a, 0x33, 0x31, 0x3a,
0x30, 0x37, 0x00, 0x65, 0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xff};

	struct bit bit;
	ck_assert_int_eq(bit_init(&bit, data, sizeof(data)), 0);
	ck_assert_str_eq(bit.ncd_filename, "ov3.ncd;UserID=0xFFFFFFFF");
	ck_assert_str_eq(bit.part_name, "6slx9tqg144");
	ck_assert_str_eq(bit.date, "2014/11/10");
	ck_assert_str_eq(bit.time, "16:31:07");
	ck_assert_uint_eq(bit.bit_length, 4);
}
END_TEST

Suite* range_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("bit");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_bit_init1);
	tcase_add_test(tc_core, test_bit_init2);
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
