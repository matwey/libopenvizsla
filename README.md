[![CMake](https://github.com/matwey/libopenvizsla/actions/workflows/cmake.yml/badge.svg)](https://github.com/matwey/libopenvizsla/actions/workflows/cmake.yml)

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
* [libusb] - USB access library;
* [libzip] - C library for reading, creating, and modifying zip archives;

Then, the library can be compiled as the following:
```sh
mkdir build && cd build
cmake ..
make all test
```

### Building with vcpkg
The dependencies can also be installed with [vcpkg] in manifest mode:
```sh
vcpkg install --triplet <triplet>
```
Then, configure CMake with the vcpkg toolchain:
```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=<triplet>
cmake --build build
```

## Development
Any pull-requests to the project are always welcome.

[check]:http://check.sourceforge.net/
[cmake]:http://www.cmake.org/
[gperf]:https://www.gnu.org/software/gperf/
[libftdi]:https://www.intra2net.com/en/developer/libftdi/
[libusb]:https://libusb.info/
[libzip]:https://libzip.org/
[vcpkg]:https://vcpkg.io/
