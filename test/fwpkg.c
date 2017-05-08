#include <check.h>
#include <stdlib.h>

#include <fwpkg.h>

START_TEST (test_fwpkg_new1) {
	struct fwpkg* fwpkg = fwpkg_new();
	ck_assert_ptr_ne(fwpkg, NULL);
	fwpkg_free(fwpkg);
}
END_TEST
START_TEST (test_fwpkg_load1) {
	struct fwpkg* fwpkg = fwpkg_new();
	int ret;
	ck_assert_ptr_ne(fwpkg, NULL);
	ret = fwpkg_from_file(fwpkg, PROJECT_ROOT "/ov3.fwpkg");
	ck_assert_int_eq(ret, 0);
	fwpkg_free(fwpkg);
}
END_TEST
START_TEST (test_fwpkg_size1) {
	struct fwpkg* fwpkg = fwpkg_new();
	int ret;
	ck_assert_ptr_ne(fwpkg, NULL);
	ret = fwpkg_from_file(fwpkg, PROJECT_ROOT "/ov3.fwpkg");
	ck_assert_int_eq(ret, 0);
	ck_assert_uint_eq(fwpkg_map_size(fwpkg), 2080);
	ck_assert_uint_eq(fwpkg_bitstream_size(fwpkg), 340972);
	fwpkg_free(fwpkg);
}
END_TEST
START_TEST (test_fwpkg_read1) {
	char* buf;
	size_t size;
	struct fwpkg* fwpkg = fwpkg_new();
	int ret;
	ck_assert_ptr_ne(fwpkg, NULL);
	ret = fwpkg_from_file(fwpkg, PROJECT_ROOT "/ov3.fwpkg");
	ck_assert_int_eq(ret, 0);
	size = fwpkg_map_size(fwpkg);
	buf = malloc(size);
	ck_assert_int_eq(fwpkg_read_map(fwpkg, buf, &size), 0);
	ck_assert_uint_eq(size, fwpkg_map_size(fwpkg));
	free(buf);
	fwpkg_free(fwpkg);
}
END_TEST
START_TEST (test_fwpkg_read2) {
	char* buf;
	size_t size;
	struct fwpkg* fwpkg = fwpkg_new();
	int ret;
	ck_assert_ptr_ne(fwpkg, NULL);
	ret = fwpkg_from_file(fwpkg, PROJECT_ROOT "/ov3.fwpkg");
	ck_assert_int_eq(ret, 0);
	size = fwpkg_bitstream_size(fwpkg);
	buf = malloc(size);
	ck_assert_int_eq(fwpkg_read_bitstream(fwpkg, buf, &size), 0);
	ck_assert_uint_eq(size, fwpkg_bitstream_size(fwpkg));
	free(buf);
	fwpkg_free(fwpkg);
}
END_TEST

Suite* range_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("fwpkg");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_fwpkg_new1);
	tcase_add_test(tc_core, test_fwpkg_load1);
	tcase_add_test(tc_core, test_fwpkg_size1);
	tcase_add_test(tc_core, test_fwpkg_read1);
	tcase_add_test(tc_core, test_fwpkg_read2);
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

