cmake_minimum_required (VERSION 3.0)
project(libopenvizsla LANGUAGES C VERSION 0.0.0)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(CheckSymbolExists)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)
pkg_check_modules(LIBFTDI REQUIRED libftdi1)
pkg_check_modules(LIBZIP REQUIRED libzip>=1.0)

set(CMAKE_C_FLAGS "-std=c99 ${LIBFTDI_C_FLAGS} ${LIBZIP_C_FLAGS}")

include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${LIBFTDI_INCLUDE_DIRS}")
if(LIBZIP_INCLUDE_DIRS)
include_directories ("${LIBZIP_INCLUDE_DIRS}")
endif(LIBZIP_INCLUDE_DIRS)

add_custom_command(OUTPUT ov3.fwpkg.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ld -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o ov3.fwpkg
	COMMAND objcopy --rename-section .data=.rodata,contents,readonly ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o ${CMAKE_CURRENT_BINARY_DIR}/ov3.fwpkg.o
	MAIN_DEPENDENCY ov3.fwpkg)

file(GLOB_RECURSE SOURCES src/*.c)
add_library(openvizsla SHARED ${SOURCES} ov3.fwpkg.o)
target_link_libraries(openvizsla ${LIBFTDI_LIBRARIES} ${LIBZIP_LIBRARIES})
set_target_properties(openvizsla PROPERTIES VERSION 0.0.0 SOVERSION 0)

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
	if(CHECK_INCLUDE_DIRS)
		target_include_directories(${test_name} ${CHECK_INCLUDE_DIRS})
	endif(CHECK_INCLUDE_DIRS)
	target_link_libraries(${test_name} ${CHECK_LDFLAGS} openvizsla)
	set_target_properties(${test_name} PROPERTIES COMPILE_FLAGS -DPROJECT_ROOT='"${PROJECT_SOURCE_DIR}"')
	add_test(${test_name} ${test_name})
endforeach(test_source)

install(TARGETS openvizsla LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
install(DIRECTORY include/
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/openvizsla
	FILES_MATCHING PATTERN "*.h")
configure_file("openvizsla.pc.in" "openvizsla.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openvizsla.pc"
	DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")
