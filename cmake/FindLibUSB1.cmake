find_package(PkgConfig)

if (PkgConfig_FOUND)
	pkg_check_modules(PC_LIBUSB1 QUIET libusb-1.0)
endif()

if (PC_LIBUSB1_FOUND)
	add_library(LibUSB1::usb INTERFACE IMPORTED)
	set_target_properties(LibUSB1::usb PROPERTIES
		INTERFACE_LINK_LIBRARIES "${PC_LIBUSB1_LDFLAGS}"
		INTERFACE_COMPILE_OPTIONS "${PC_LIBUSB1_CFLAGS}")

	set(LIBUSB1_INCLUDE_DIRS "${PC_LIBUSB1_INCLUDE_DIRS}")
	set(LIBUSB1_LIBRARIES "${PC_LIBUSB1_LIBRARIES}")
	set(LIBUSB1_VERSION_STRING "${PC_LIBUSB1_VERSION}")
endif()

if (NOT TARGET LibUSB1::usb)
	find_path(LIBUSB1_INCLUDE_DIRS libusb.h PATH_SUFFIXES libusb-1.0)
	find_library(LIBUSB1_LIBRARIES NAMES usb-1.0 libusb-1.0)

	file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c"
		"#include<libusb.h>\n#include<stdio.h>\nint main(){const struct libusb_version* v=libusb_get_version();printf(\"%d.%d.%d%s\",v->major,v->minor,v->micro,v->rc);return 0;}")

	try_run(_result_var _compile_result_var
		${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c
		CMAKE_FLAGS -DINCLUDE_DIRECTORIES:STRING=${LIBUSB1_INCLUDE_DIRS} -DLINK_LIBRARIES:STRING=${LIBUSB1_LIBRARIES}
		RUN_OUTPUT_VARIABLE LIBUSB1_VERSION_STRING)

	unset(_result_var)
	unset(_compile_result_var)

	add_library(LibUSB1::usb INTERFACE IMPORTED)
	set_target_properties(LibUSB1::usb PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB1_INCLUDE_DIRS}"
		INTERFACE_LINK_LIBRARIES "${LIBUSB1_LIBRARIES}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibUSB1
	REQUIRED_VARS LIBUSB1_LIBRARIES
	VERSION_VAR LIBUSB1_VERSION_STRING)

mark_as_advanced(LIBUSB1_INCLUDE_DIRS LIBUSB1_LIBRARIES)
