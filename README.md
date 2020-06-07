[![Build Status](https://semaphoreci.com/api/v1/matwey/libopenvizsla/branches/master/badge.svg)](https://semaphoreci.com/matwey/libopenvizsla)
[![Build status](https://ci.appveyor.com/api/projects/status/30dt9205mvd35i8l/branch/master?svg=true)](https://ci.appveyor.com/project/matwey/libopenvizsla/branch/master)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/matwey/libopenvizsla.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/matwey/libopenvizsla/alerts/)

# libopenvizsla
An attempt to reimplement [OpenVizsla](http://openvizsla.org/) host software in plain C.

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
