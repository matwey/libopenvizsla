/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <fwpkg.h>

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <Windows.h>

/* It is ok to have NULL hinstance in case of static linkage */
static HINSTANCE hinstance = NULL;

BOOL DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
	hinstance = hinstDll;
	return TRUE;
}
#else
extern const char _binary_ov3_fwpkg_start[];
extern const char _binary_ov3_fwpkg_end[];
#endif

static int fwpkg_read_file(struct fwpkg* fwpkg, void* buf, size_t* size, zip_uint64_t index) {
	struct zip_file* file = zip_fopen_index(fwpkg->pkg, index, 0);
	zip_int64_t ret = 0;
	zip_int64_t nbytes = *size;

	if (!file) {
		fwpkg->error_str = zip_strerror(fwpkg->pkg);
		return -1;
	}

	ret = zip_fread(file, buf, nbytes);
	if (ret == -1) {
		fwpkg->error_str = zip_file_strerror(file);

		zip_fclose(file);
		return -1;
	}
	*size = ret;

	zip_fclose(file);

	return 0;
}

static size_t fwpkg_file_size(struct fwpkg* fwpkg, zip_uint64_t index) {
	int ret;
	struct zip_stat sb;

	ret = zip_stat_index(fwpkg->pkg, index, 0, &sb);
	if (ret == -1) {
		fwpkg->error_str = zip_strerror(fwpkg->pkg);

		return (size_t)(-1);
	}

	if (!(sb.valid & ZIP_STAT_SIZE)) {
		fwpkg->error_str = "Can not read uncompressed size";

		return (size_t)(-1);
	}

	return (size_t)(sb.size);
}

static int fwpkg_locate_files(struct fwpkg* fwpkg) {
	fwpkg->map_index = zip_name_locate(fwpkg->pkg, "map.txt", ZIP_FL_NOCASE | ZIP_FL_NODIR);
	if (fwpkg->map_index == -1) {
		fwpkg->error_str = "Can not find map.txt file";

		zip_discard(fwpkg->pkg);
		return -1;
	}

	fwpkg->bitstream_index = zip_name_locate(fwpkg->pkg, "ov3.bit", ZIP_FL_NOCASE | ZIP_FL_NODIR);
	if (fwpkg->bitstream_index == -1) {
		fwpkg->error_str = "Can not find ov3.bit file";

		zip_discard(fwpkg->pkg);
		return -1;
	}

	return 0;
}

static int fwpkg_init_from_buffer(struct fwpkg* fwpkg, const void* data, size_t size) {
	zip_error_t error;
	zip_source_t *src = NULL;

	memset(fwpkg, 0, sizeof(struct fwpkg));

	src = zip_source_buffer_create(data, size, 0, &error);
	if (!src) {
		fwpkg->error_str = zip_error_strerror(&error);
		return -1;
	}

	fwpkg->pkg = zip_open_from_source(src, ZIP_CHECKCONS | ZIP_RDONLY, &error);
	if (!fwpkg->pkg) {
		fwpkg->error_str = zip_error_strerror(&error);
		return -1;
	}

	return fwpkg_locate_files(fwpkg);
}

int fwpkg_init(struct fwpkg* fwpkg, const char* filename) {
	return (filename ? fwpkg_init_from_file(fwpkg, filename) : fwpkg_init_from_preload(fwpkg));
}

int fwpkg_init_from_file(struct fwpkg* fwpkg, const char* filename) {
	zip_error_t ze;
	int error;

	memset(fwpkg, 0, sizeof(struct fwpkg));

	fwpkg->pkg = zip_open(filename, ZIP_CHECKCONS | ZIP_RDONLY, &error);
	if (!fwpkg->pkg) {
		zip_error_init_with_code(&ze, error);
		fwpkg->error_str = zip_error_strerror(&ze);
		return -1;
	}

	return fwpkg_locate_files(fwpkg);
}

#if defined(_WIN32) || defined(__CYGWIN__)
int fwpkg_init_from_preload(struct fwpkg* fwpkg) {
	HGLOBAL res_handle = NULL;
	HRSRC res;
	void* data;
	DWORD size;

	res = FindResource(hinstance, MAKEINTRESOURCE(OV_FWPKG_RESOURCE), RT_RCDATA);
	if (!res) {
		fwpkg->error_str = "Cannot find OV_FWPKG_RESOURCE resource";
		return -1;
	}

	res_handle = LoadResource(hinstance, res);
	if (!res_handle) {
		fwpkg->error_str = "Cannot load OV_FWPKG_RESOURCE resource";
		return -1;
	}

	data = LockResource(res_handle);
	size = SizeofResource(hinstance, res);

	return fwpkg_init_from_buffer(fwpkg, data, size);
}
#else
int fwpkg_init_from_preload(struct fwpkg* fwpkg) {
	return fwpkg_init_from_buffer(fwpkg,
		(const void*)_binary_ov3_fwpkg_start,
		(const void*)_binary_ov3_fwpkg_end - (const void*)_binary_ov3_fwpkg_start);
}
#endif

void fwpkg_destroy(struct fwpkg* fwpkg) {
	zip_discard(fwpkg->pkg);
}

const char* fwpkg_get_error_string(struct fwpkg* fwpkg) {
	return fwpkg->error_str;
}

int fwpkg_read_map(struct fwpkg* fwpkg, void* buf, size_t* size) {
	return fwpkg_read_file(fwpkg, buf, size, fwpkg->map_index);
}

int fwpkg_read_bitstream(struct fwpkg* fwpkg, void* buf, size_t* size) {
	return fwpkg_read_file(fwpkg, buf, size, fwpkg->bitstream_index);
}

size_t fwpkg_map_size(struct fwpkg* fwpkg) {
	return fwpkg_file_size(fwpkg, fwpkg->map_index);
}

size_t fwpkg_bitstream_size(struct fwpkg* fwpkg) {
	return fwpkg_file_size(fwpkg, fwpkg->bitstream_index);
}

