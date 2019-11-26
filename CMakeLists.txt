cmake_minimum_required (VERSION 3.1)
# CXX language is required by GenerateExportHeader
project(libopenvizsla LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(CheckSymbolExists)
include(GNUInstallDirs)
include(GenerateExportHeader)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFTDI REQUIRED libftdi1)
pkg_check_modules(LIBZIP REQUIRED libzip>=1.0)

find_package(Check CONFIG NAMES Check check)
if (NOT TARGET Check::check)
	find_package(PkgConfig REQUIRED)

	pkg_search_module(CHECK REQUIRED Check check)

	add_library(Check::check INTERFACE IMPORTED)
	set_target_properties(Check::check PROPERTIES
		INTERFACE_LINK_LIBRARIES "${CHECK_LDFLAGS}"
		INTERFACE_COMPILE_OPTIONS "${CHECK_CFLAGS}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED ${LIBFTDI_C_FLAGS} ${LIBZIP_C_FLAGS}")

include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${LIBFTDI_INCLUDE_DIRS}")
if(LIBZIP_INCLUDE_DIRS)
include_directories ("${LIBZIP_INCLUDE_DIRS}")
endif(LIBZIP_INCLUDE_DIRS)

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
	${LIBFTDI_LDFLAGS}
	${LIBZIP_LDFLAGS})

add_library(openvizsla ${SOURCES})
target_link_libraries(openvizsla ${LIBRARIES})
set_target_properties(openvizsla PROPERTIES
	VERSION 0.0.0
	SOVERSION 0)
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
	target_link_libraries(openvizsla_static ${LIBRARIES})
	set_target_properties(openvizsla_static PROPERTIES
		COMPILE_FLAGS -DOPENVIZSLA_STATIC_DEFINE
		ARCHIVE_OUTPUT_NAME openvizsla)

	if(WIN32)
		target_sources(openvizsla PRIVATE ${WIN32_RESOURCE_FILE})
		target_sources(openvizsla_static PUBLIC ${WIN32_RESOURCE_FILE})
	endif()
else()
	add_library(openvizsla_static ALIAS openvizsla)

	if(WIN32)
		target_sources(openvizsla PUBLIC ${WIN32_RESOURCE_FILE})
	endif()
endif()

file(GLOB TOOLS tools/*)
foreach(tool_target IN ITEMS ${TOOLS})
        file(GLOB_RECURSE SOURCES ${tool_target}/*.c)
        get_filename_component(tool_name ${tool_target} NAME)
        add_executable(${tool_name} ${SOURCES})
        target_link_libraries(${tool_name} openvizsla)
endforeach(tool_target)

enable_testing()
file(GLOB_RECURSE TESTS test/*.c)
foreach(test_source IN ITEMS ${TESTS})
	string(REPLACE ${CMAKE_SOURCE_DIR}/ "" test_name ${test_source})
	string(REPLACE / _ test_name ${test_name})
	string(REPLACE .c "" test_name ${test_name})
	add_executable(${test_name} ${test_source})
	target_link_libraries(${test_name} Check::check openvizsla_static)
	set_target_properties(${test_name} PROPERTIES COMPILE_FLAGS -DPROJECT_ROOT='"${PROJECT_SOURCE_DIR}"')
	add_test(${test_name} ${test_name})
endforeach(test_source)

install(TARGETS openvizsla
	DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
install(FILES "include/ov.h"
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/openvizsla)
configure_file("openvizsla.pc.in" "openvizsla.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openvizsla.pc"
	DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")
