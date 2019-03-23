#include <ov.h>

int ov_init(struct ov_device* ov) {
	int ret = 0;

	ret = cha_init(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto error_cha_init;
	}

	ret = chb_init(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto error_chb_init;
	}

	return 0;

error_chb_init:
	cha_destroy(&ov->cha);
error_cha_init:

	return ret;
}

int ov_open(struct ov_device* ov) {
	int ret = 0;

	ret = cha_open(&ov->cha);
	if (ret < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto error_cha_open;
	}

	ret = chb_open(&ov->chb);
	if (ret < 0) {
		ov->error_str = chb_get_error_string(&ov->chb);
		goto error_chb_open;
	}

	return 0;

error_chb_open:
	// FIXME: close cha?
error_cha_open:

	return ret;
}

void ov_destroy(struct ov_device* ov) {
	chb_destroy(&ov->chb);
	cha_destroy(&ov->cha);
}

int ov_capture_start(struct ov_device* ov, struct packet* packet, size_t packet_size, packet_decoder_callback callback, void* user_data) {

	if (cha_switch_fifo_mode(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_switch_fifo_mode;
	}

	if (cha_stop_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_stop_stream;
	}

	if (cha_write_reg(&ov->cha, 0x402, 0x4a) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_ucfg_wdata_wr;
	}

	if (cha_write_reg(&ov->cha, 0x403, 0x80 | (0x04 & 0x3F)) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_ucfg_wcmd_wr;
	}

	if (cha_loop_init(&ov->loop, &ov->cha, packet, packet_size, callback, user_data) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_loop_init;
	}

	if (cha_start_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		goto fail_cha_start_stream;
	}

	return 0;

fail_cha_start_stream:
fail_cha_loop_init:
fail_ucfg_wcmd_wr:
fail_ucfg_wdata_wr:
fail_cha_stop_stream:
fail_cha_switch_fifo_mode:
	return -1;
}

int ov_capture_dispatch(struct ov_device* ov, int count) {
	int ret = 0;

	if ((ret = cha_loop_run(&ov->loop, count)) == -1) {
		ov->error_str = cha_get_error_string(&ov->cha);
	}

	return ret;
}

int ov_capture_stop(struct ov_device* ov) {
	if (cha_stop_stream(&ov->cha) < 0) {
		ov->error_str = cha_get_error_string(&ov->cha);
		return -1;
	}

	return 0;
}

const char* ov_get_error_string(struct ov_device* ov) {
	return ov->error_str;
}
