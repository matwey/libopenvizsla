[![CMake](https://github.com/matwey/libopenvizsla/actions/workflows/cmake.yml/badge.svg)](https://github.com/matwey/libopenvizsla/actions/workflows/cmake.yml)
[![Build Status](https://semaphoreci.com/api/v1/matwey/libopenvizsla/branches/master/badge.svg)](https://semaphoreci.com/matwey/libopenvizsla)
[![CodeQL](https://github.com/matwey/libopenvizsla/actions/workflows/codeql.yml/badge.svg)](https://github.com/matwey/libopenvizsla/actions/workflows/codeql.yml)

# libopenvizsla
An attempt to reimplement [OpenVizsla](http://openvizsla.org/) host software in plain C.

## Getting Started
This section describes how to get started with OpenVizsla and Wireshark.

### Linux
1. Build the project as described below
2. Copy the `ovextcap` executable to Wiresharks `extcap` directory
3. Run Wireshark and start capturing

### Windows
1. Use `Zadig` to install the `libusbK` driver for OpenVizsla
2. Download Windows release
3. Extract and copy all files from `bin` directory to Wiresharks `extcap` directory
4. Run Wireshark and start capturing

## Building
Following components are required to build libopenvizsla:
* [check] - unit testing framework for C;
* [cmake] - cross-platform open-source build system;
* [gperf] - a perfect hash function generator;
* [libftdi] - FTDI USB userspace driver;
* [libzip] - C library for reading, creating, and modifying zip archives;

Then, the library can be compiled as the following:
```sh
mkdir build && cd build
cmake ..
make all test
```

## Development
Any pull-requests to the project are always welcome.

[check]:http://check.sourceforge.net/
[cmake]:http://www.cmake.org/
[gperf]:https://www.gnu.org/software/gperf/
[libftdi]:https://www.intra2net.com/en/developer/libftdi/
[libzip]:https://libzip.org/
