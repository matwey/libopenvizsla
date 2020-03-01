cmake_minimum_required (VERSION 3.1)
# CXX language is required by GenerateExportHeader
project(libopenvizsla LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_C_STANDARD 99)

include(CheckSymbolExists)
include(GNUInstallDirs)
include(GenerateExportHeader)

find_package(LibUSB1 REQUIRED)
find_package(LibFTDI1 REQUIRED)
find_package(LibZip 1.0 REQUIRED)
find_package(Check CONFIG NAMES Check check)
if (NOT TARGET Check::check)
	find_package(PkgConfig REQUIRED)

	pkg_search_module(CHECK REQUIRED Check check)

	add_library(Check::check INTERFACE IMPORTED)
	set_target_properties(Check::check PROPERTIES
		INTERFACE_LINK_LIBRARIES "${CHECK_LDFLAGS}"
		INTERFACE_COMPILE_OPTIONS "${CHECK_CFLAGS}")
endif()

include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(OUTPUT ov3.fwpkg.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ld -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o ov3.fwpkg
	COMMAND objcopy --rename-section .data=.rodata,contents,readonly ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o
	MAIN_DEPENDENCY ov3.fwpkg)

find_program(GPERF NAMES gperf)
if(NOT GPERF)
	message(FATAL_ERROR "Could not find gperf tool")
endif()

add_custom_command(OUTPUT reg_gperf.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${GPERF} -t -L C src/reg.gperf > ${CMAKE_CURRENT_BINARY_DIR}/reg_gperf.h
	MAIN_DEPENDENCY src/reg.gperf)

file(GLOB_RECURSE SOURCES src/*.c)
list(APPEND SOURCES
	${CMAKE_CURRENT_BINARY_DIR}/reg_gperf.h)
list(APPEND LIBRARIES
	LibUSB1::usb
	LibFTDI1::ftdi1
	LibZip::zip)

add_library(openvizsla ${SOURCES})
target_link_libraries(openvizsla ${LIBRARIES})
set_target_properties(openvizsla PROPERTIES
	VERSION 0.0.0
	SOVERSION 0)
target_compile_definitions(openvizsla PUBLIC "_XOPEN_SOURCE;_XOPEN_SOURCE_EXTENDED")

generate_export_header(openvizsla)

if(NOT WIN32)
	target_sources(openvizsla PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o)
else()
	set(WIN32_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/fwpkg.rc")
endif()

get_target_property(OPENVIZSLA_LIBRARY_TYPE openvizsla TYPE)
if(NOT ${OPENVIZSLA_LIBRARY_TYPE} STREQUAL "STATIC_LIBRARY")
	set_target_properties(openvizsla PROPERTIES
		C_VISIBILITY_PRESET hidden)

	add_library(openvizsla_static STATIC EXCLUDE_FROM_ALL ${SOURCES})
	target_link_libraries(openvizsla_static LibZip::zip ${LIBRARIES})
	set_target_properties(openvizsla_static PROPERTIES
		ARCHIVE_OUTPUT_NAME openvizsla)
	target_compile_definitions(openvizsla_static PUBLIC "OPENVIZSLA_STATIC_DEFINE;_XOPEN_SOURCE;_XOPEN_SOURCE_EXTENDED")

	if(NOT WIN32)
		target_sources(openvizsla_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o)
	else()
		target_sources(openvizsla PRIVATE ${WIN32_RESOURCE_FILE})
		target_sources(openvizsla_static PUBLIC ${WIN32_RESOURCE_FILE})
	endif()
else()
	add_library(openvizsla_static ALIAS openvizsla)

	if(WIN32)
		target_sources(openvizsla PUBLIC ${WIN32_RESOURCE_FILE})
	endif()
endif()

check_symbol_exists(getopt_long "getopt.h" HAVE_GETOPT)
if (NOT HAVE_GETOPT)
	find_path(GETOPT_INCLUDE_DIRS getopt.h)
	find_library(GETOPT_LIBRARIES getopt)

	add_library(getopt::getopt INTERFACE IMPORTED)
	set_target_properties(getopt::getopt PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${GETOPT_INCLUDE_DIRS}"
		INTERFACE_LINK_LIBRARIES "${GETOPT_LIBRARIES}")
endif()

file(GLOB TOOLS tools/*)
foreach(tool_target IN ITEMS ${TOOLS})
	file(GLOB_RECURSE SOURCES ${tool_target}/*.c)
	get_filename_component(tool_name ${tool_target} NAME)
	add_executable(${tool_name} ${SOURCES})
	list(APPEND _libraries openvizsla)
	if (TARGET getopt::getopt)
		list(APPEND _libraries getopt::getopt)
	endif()
	target_link_libraries(${tool_name} ${_libraries})
	unset(_libraries)
endforeach(tool_target)

enable_testing()
file(GLOB_RECURSE TESTS test/*.c)
foreach(test_source IN ITEMS ${TESTS})
	string(REPLACE ${CMAKE_SOURCE_DIR}/ "" test_name ${test_source})
	string(REPLACE / _ test_name ${test_name})
	string(REPLACE .c "" test_name ${test_name})
	add_executable(${test_name} ${test_source})
	target_link_libraries(${test_name} Check::check openvizsla_static)
	target_compile_definitions(${test_name} PRIVATE PROJECT_ROOT="${PROJECT_SOURCE_DIR}")
	add_test(${test_name} ${test_name})
endforeach(test_source)

install(TARGETS openvizsla
	DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
install(FILES "include/ov.h"
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/openvizsla)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openvizsla_export.h"
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/openvizsla)
configure_file("openvizsla.pc.in" "openvizsla.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openvizsla.pc"
	DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")
