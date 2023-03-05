#include <check.h>
#include <stdlib.h>

#include <decoder.h>

union {
	struct ov_packet packet;
	uint8_t data[1024];
} p;
struct packet_decoder pd;
struct frame_decoder fd;

void callback(struct ov_packet* packet, void* data) {
}

void packet_setup() {
	ck_assert_int_eq(packet_decoder_init(&pd, &p.packet, sizeof(p), &callback, NULL), 0);
}

void packet_teardown() {

}

void frame_setup() {
	ck_assert_int_eq(frame_decoder_init(&fd, &p.packet, sizeof(p), &callback, NULL), 0);
}

void frame_teardown() {

}

START_TEST (test_packet_decoder1) {
	char inp[] = {0xa0,0,0,0x01,0,0xc4,0xcc,0x96,0x5a};
	ck_assert_int_eq(packet_decoder_proc(&pd, inp, sizeof(inp)), sizeof(inp));
	ck_assert_int_eq(pd.state, NEED_PACKET_MAGIC);
	ck_assert_int_eq(p.packet.size, 1);
	ck_assert_int_eq(p.packet.timestamp, 0x96ccc4);
	ck_assert_int_eq(memcmp(p.packet.data, inp+8, p.packet.size), 0);
}
END_TEST

START_TEST (test_packet_decoder2) {
	char inp[] = {0xa0,0,0,0x01,0,0xc4,0xcc,0x96,0x5a,0xa0};
	ck_assert_int_eq(packet_decoder_proc(&pd, inp, sizeof(inp)), sizeof(inp)-1);
	ck_assert_int_eq(pd.state, NEED_PACKET_MAGIC);
	ck_assert_int_eq(p.packet.size, 1);
	ck_assert_int_eq(p.packet.timestamp, 0x96ccc4);
	ck_assert_int_eq(memcmp(p.packet.data, inp+8, p.packet.size), 0);
}
END_TEST

START_TEST (test_packet_decoder3) {
	char inp[] = {1,2,3};
	ck_assert_int_eq(packet_decoder_proc(&pd, inp, sizeof(inp)), -1);
}
END_TEST

START_TEST (test_packet_decoder4) {
	char inp[] = {0xa0,0,0,0x03,0,0xac,0x6c,0xa5,0x69,0x83,0xe0};
	ck_assert_int_eq(packet_decoder_proc(&pd, inp, sizeof(inp)), sizeof(inp));
	ck_assert_int_eq(pd.state, NEED_PACKET_MAGIC);
	ck_assert_int_eq(p.packet.size, 3);
	ck_assert_int_eq(p.packet.timestamp, 0xa56cac);
	ck_assert_int_eq(memcmp(p.packet.data, inp+8, p.packet.size), 0);
}
END_TEST

START_TEST (test_packet_decoder5) {
	char inp1[] = {0xa0,0,0,0x03,0,0xac};
	char inp2[] = {0x6c,0xa5,0x69,0x83,0xe0};
	ck_assert_int_eq(packet_decoder_proc(&pd, inp1, sizeof(inp1)), sizeof(inp1));
	ck_assert_int_eq(pd.state, NEED_PACKET_TIMESTAMP_ME);
	ck_assert_int_eq(p.packet.size, 3);
	ck_assert_int_eq(packet_decoder_proc(&pd, inp2, sizeof(inp2)), sizeof(inp2));
	ck_assert_int_eq(pd.state, NEED_PACKET_MAGIC);
	ck_assert_int_eq(p.packet.timestamp, 0xa56cac);
	ck_assert_int_eq(memcmp(p.packet.data, inp2+2, p.packet.size), 0);
}
END_TEST

START_TEST (test_frame_decoder1) {
	char inp[] = {
		0xd0, 0x1f, 0xa0, 0x00, 0x00, 0x03, 0x00, 0xba,
		0x6e, 0x6e, 0x69, 0xd7, 0x60, 0xa0, 0x00, 0x00,
		0x01, 0x00, 0x22, 0x75, 0x6e, 0x5a, 0xa0, 0x00,
		0x00, 0x03, 0x00, 0xe2, 0xc1, 0x75, 0x69, 0xd7,
		0x60, 0xa0, 0x00, 0x00, 0x01, 0x00, 0x4a, 0xc8,
		0x75, 0x5a, 0xa0, 0x00, 0x00, 0x03, 0x00, 0x0a,
		0x15, 0x7d, 0x69, 0xd7, 0x60, 0xa0, 0x00, 0x00,
		0x01, 0x00, 0x72, 0x1b, 0x7d, 0x5a, 0xa0, 0x00,
		0x00, 0x03
	}; 

	ck_assert_int_eq(frame_decoder_proc(&fd, inp, sizeof(inp)), sizeof(inp));
	ck_assert_int_eq(fd.state, NEED_FRAME_MAGIC);
}
END_TEST

START_TEST (test_frame_decoder2) {
	char inp[] = {
		0xd0, 0x1f, 0xa0, 0x00, 0x00, 0x03, 0x00, 0xba,
		0x6e, 0x6e, 0x69, 0xd7, 0x60, 0xa0, 0x00, 0x00,
		0x01, 0x00, 0x22, 0x75, 0x6e, 0x5a, 0xa0, 0x00,
		0x00, 0x03, 0x00, 0xe2, 0xc1, 0x75, 0x69, 0xd7,
		0x60, 0xa0, 0x00, 0x00, 0x01, 0x00, 0x4a, 0xc8,
		0x75, 0x5a, 0xa0, 0x00, 0x00, 0x03, 0x00, 0x0a,
		0x15, 0x7d, 0x69, 0xd7, 0x60, 0xa0, 0x00, 0x00,
		0x01, 0x00, 0x72, 0x1b, 0x7d, 0x5a, 0xa0, 0x00,
		0x00, 0x03, 0xd0, 0x03, 0x00, 0x0a, 0x15, 0x7d,
		0x69, 0xd7, 0x60, 0xa0
	}; 

	ck_assert_int_eq(frame_decoder_proc(&fd, inp, sizeof(inp)), sizeof(inp));
	ck_assert_int_eq(fd.state, NEED_FRAME_MAGIC);
}
END_TEST

START_TEST (test_frame_decoder3) {
	char inp[] = {
		0x55, 0x8c, 0x28, 0x00, 0x09, 0xd0, 0x1f, 0xa0,
		0x00, 0x00, 0x03, 0x00, 0xba, 0x6e, 0x6e, 0x69,
		0xd7, 0x60, 0xa0, 0x00, 0x00, 0x01, 0x00, 0x22,
		0x75, 0x6e, 0x5a, 0xa0, 0x00, 0x00, 0x03, 0x00,
		0xe2, 0xc1, 0x75, 0x69, 0xd7, 0x60, 0xa0, 0x00,
		0x00, 0x01, 0x00, 0x4a, 0xc8, 0x75, 0x5a, 0xa0,
		0x00, 0x00, 0x03, 0x00, 0x0a, 0x15, 0x7d, 0x69,
		0xd7, 0x60, 0xa0, 0x00, 0x00, 0x01, 0x00, 0x72,
		0x1b, 0x7d, 0x5a, 0xa0, 0x00, 0x00, 0x03
	};

	ck_assert_int_eq(frame_decoder_proc(&fd, inp, sizeof(inp)), sizeof(inp));
	ck_assert_int_eq(fd.state, NEED_FRAME_MAGIC);
}
END_TEST

Suite* range_suite(void) {
	Suite *s;
	TCase *tc_packet;
	TCase *tc_frame;

	s = suite_create("decoder");

	tc_packet = tcase_create("Packet");
	tcase_add_checked_fixture(tc_packet, packet_setup, packet_teardown);
	tcase_add_test(tc_packet, test_packet_decoder1);
	tcase_add_test(tc_packet, test_packet_decoder2);
	tcase_add_test(tc_packet, test_packet_decoder3);
	tcase_add_test(tc_packet, test_packet_decoder4);
	tcase_add_test(tc_packet, test_packet_decoder5);
	suite_add_tcase(s, tc_packet);

	tc_frame = tcase_create("Frame");
	tcase_add_checked_fixture(tc_frame, frame_setup, frame_teardown);
	tcase_add_test(tc_frame, test_frame_decoder1);
	tcase_add_test(tc_frame, test_frame_decoder2);
	tcase_add_test(tc_frame, test_frame_decoder3);
	suite_add_tcase(s, tc_frame);

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

