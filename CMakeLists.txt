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
add_executable(openvizsla ${SOURCES})
target_link_libraries(openvizsla ${LIBFTDI_LIBRARIES} ${LIBZIP_LIBRARIES})
