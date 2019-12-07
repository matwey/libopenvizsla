
find_package(PkgConfig)

if (PkgConfig_FOUND)
	pkg_check_modules(PC_LIBZIP QUIET libzip)
endif()

if (PC_LIBZIP_FOUND)
	add_library(LibZip::zip INTERFACE IMPORTED)
	set_target_properties(LibZip::zip PROPERTIES
		INTERFACE_LINK_LIBRARIES "${PC_LIBZIP_LDFLAGS}"
		INTERFACE_COMPILE_OPTIONS "${PC_LIBZIP_CFLAGS}")

	set(LIBZIP_INCLUDE_DIRS "${PC_LIBZIP_INCLUDE_DIRS}")
	set(LIBZIP_LIBRARIES "${PC_LIBZIP_LIBRARIES}")
	set(LIBZIP_VERSION_STRING "${PC_LIBZIP_VERSION}")
endif()

if (NOT TARGET LibZip::zip)
	find_path(LIBZIP_INCLUDE_DIRS zip.h)
	find_path(LIBZIP_ZIPCONF_DIR zipconf.h)
	find_library(LIBZIP_LIBRARIES zip)

	file(STRINGS "${LIBZIP_ZIPCONF_DIR}/zipconf.h" libzip_version_str REGEX "^#define[ \t]+LIBZIP_VERSION[ \t]+\".+\"")
	string(REGEX REPLACE "^#define[ \t]+LIBZIP_VERSION[ \t]+\"([^\"]+)\".*" "\\1" LIBZIP_VERSION_STRING "${libzip_version_str}")
	unset(libzip_version_str)

	add_library(LibZip::zip INTERFACE IMPORTED)
	set_target_properties(LibZip::zip PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIRS}"
		INTERFACE_LINK_LIBRARIES "${LIBZIP_LIBRARIES}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibZip
	REQUIRED_VARS LIBZIP_LIBRARIES
	VERSION_VAR LIBZIP_VERSION_STRING)

mark_as_advanced(LIBZIP_INCLUDE_DIRS LIBZIP_LIBRARIES)
