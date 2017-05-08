cmake_minimum_required (VERSION 2.8)
project(libopenvizsla C)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(CheckSymbolExists)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)
pkg_check_modules(LIBFTDI REQUIRED libftdi1)
pkg_check_modules(LIBZIP REQUIRED libzip)

set(CMAKE_C_FLAGS "-std=c99 ${LIBFTDI_C_FLAGS} ${LIBZIP_C_FLAGS}")

include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${LIBFTDI_INCLUDE_DIRS}")
include_directories ("${LIBZIP_INCLUDE_DIRS}")

file(GLOB_RECURSE SOURCES src/*.c)
add_library(openvizsla SHARED ${SOURCES})
target_link_libraries(openvizsla ${LIBFTDI_LIBRARIES} ${LIBZIP_LIBRARIES})

file(GLOB TOOLS tools/*)
foreach(tool_target IN ITEMS ${TOOLS})
        file(GLOB_RECURSE SOURCES ${tool_target}/*.c)
        get_filename_component(tool_name ${tool_target} NAME)
        add_executable(${tool_name} ${SOURCES})
        target_link_libraries(${tool_name} openvizsla)
endforeach(tool_target)
