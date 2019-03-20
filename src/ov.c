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

const char* ov_get_error_string(struct ov_device* ov) {
	return ov->error_str;
}
