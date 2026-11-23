#[=======================================================================[.rst:
FindLibFTDI1
------------

Finds the libftdi-1.0 library:

.. code-block:: cmake

  find_package(LibFTDI1 [<version>] [...])

Imported Targets
^^^^^^^^^^^^^^^^

``LibFTDI1::ftdi1``
  Target encapsulating the libftdi-1.0 usage requirements.  It is available
  only if the library was found.

Result Variables
^^^^^^^^^^^^^^^^

``LibFTDI1_FOUND``
  Boolean indicating whether the (requested version of) libftdi-1.0 was found.

``LibFTDI1_VERSION``
  Version of the libftdi runtime.  It is determined by compiling and running a
  small test program that calls ``ftdi_get_library_version()``.  When cross
  compiling, either set ``CMAKE_CROSSCOMPILING_EMULATOR`` or provide the version
  manually by setting ``LibFTDI1_VERSION`` before the ``find_package()`` call.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set to override the search:

``LIBFTDI1_INCLUDE_DIR``
  Directory containing ``ftdi.h``.

``LIBFTDI1_LIBRARY``
  Path to the libftdi1 library.

Hints
^^^^^

``LibFTDI1_ROOT``
  Installation prefix to search first.

libftdi-1.0 depends on libusb-1.0, so this module also calls
``find_package(LibUSB1 QUIET)`` and adds ``LibUSB1::usb`` as a link-only
dependency of ``LibFTDI1::ftdi1``.  This is required for static libftdi builds.

``pkg-config`` is queried only to add hints to the searches above.  The imported
target is built from the paths found by CMake; ``pkg-config`` compiler and
linker flags are deliberately not propagated into it.

Examples
^^^^^^^^

Finding libftdi and linking it to a project target:

.. code-block:: cmake

  find_package(LibFTDI1 1.0 REQUIRED)
  target_link_libraries(project_target PRIVATE LibFTDI1::ftdi1)
#]=======================================================================]

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_LIBFTDI1 QUIET libftdi1)
endif()

find_path(LIBFTDI1_INCLUDE_DIR
  NAMES ftdi.h
  HINTS
    ${LibFTDI1_ROOT}
    $ENV{LibFTDI1_ROOT}
    ${PC_LIBFTDI1_INCLUDEDIR}
    ${PC_LIBFTDI1_INCLUDE_DIRS}
  PATH_SUFFIXES
    include/libftdi1
    libftdi1
    include)

find_library(LIBFTDI1_LIBRARY
  NAMES ftdi1
  HINTS
    ${LibFTDI1_ROOT}
    $ENV{LibFTDI1_ROOT}
    ${PC_LIBFTDI1_LIBDIR}
    ${PC_LIBFTDI1_LIBRARY_DIRS}
  PATH_SUFFIXES
    lib
    lib64)

# libftdi-1.0 cannot be linked without libusb-1.0, especially when using the
# static library.  Reuse the dedicated module instead of copying pkg-config
# linker flags into this target.
find_package(LibUSB1 QUIET)

set(_libftdi1_link_libraries "${LIBFTDI1_LIBRARY}")
if(LibUSB1_FOUND)
  list(APPEND _libftdi1_link_libraries "${LIBUSB1_LIBRARY}")

  # Pick up platform-specific dependencies (macOS frameworks, etc.) declared by
  # FindLibUSB1 so the version probe can link against static libusb as well.
  get_target_property(_libftdi1_usb_link_libraries LibUSB1::usb
                      INTERFACE_LINK_LIBRARIES)
  if(_libftdi1_usb_link_libraries)
    list(APPEND _libftdi1_link_libraries ${_libftdi1_usb_link_libraries})
  endif()
  unset(_libftdi1_usb_link_libraries)
endif()

# libftdi has no usable compile-time version macro, so ask the runtime.
# Keep this optional: failing to determine the version must not make the whole
# package unusable when no version was requested.
if(NOT DEFINED LibFTDI1_VERSION AND LIBFTDI1_INCLUDE_DIR AND LIBFTDI1_LIBRARY
   AND (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR))
  unset(_libftdi1_run_result CACHE)
  unset(_libftdi1_compile_result CACHE)

  set(_libftdi1_version_source
      "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/libftdi1_version.c")
  file(WRITE "${_libftdi1_version_source}" [=[
#include <ftdi.h>
#include <stdio.h>

int main(void)
{
  const struct ftdi_version_info v = ftdi_get_library_version();
  printf("%s", v.version_str);
  return 0;
}
]=])

  try_run(_libftdi1_run_result _libftdi1_compile_result
    "${CMAKE_BINARY_DIR}"
    "${_libftdi1_version_source}"
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES:STRING=${LIBFTDI1_INCLUDE_DIR}"
      "-DLINK_LIBRARIES:STRING=${_libftdi1_link_libraries}"
    RUN_OUTPUT_VARIABLE _libftdi1_version_output)

  if(_libftdi1_compile_result AND _libftdi1_run_result STREQUAL "0"
     AND _libftdi1_version_output MATCHES "^[0-9]+\\.[0-9]+")
    set(LibFTDI1_VERSION "${_libftdi1_version_output}")
  endif()

  unset(_libftdi1_run_result)
  unset(_libftdi1_compile_result)
  unset(_libftdi1_version_output)
  file(REMOVE "${_libftdi1_version_source}")
  unset(_libftdi1_version_source)
endif()

unset(_libftdi1_link_libraries)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibFTDI1
  REQUIRED_VARS LIBFTDI1_LIBRARY LIBFTDI1_INCLUDE_DIR LibUSB1_FOUND
  VERSION_VAR LibFTDI1_VERSION)

mark_as_advanced(LIBFTDI1_INCLUDE_DIR LIBFTDI1_LIBRARY)

if(LibFTDI1_FOUND AND NOT TARGET LibFTDI1::ftdi1)
  add_library(LibFTDI1::ftdi1 UNKNOWN IMPORTED)
  set_target_properties(LibFTDI1::ftdi1 PROPERTIES
    IMPORTED_LOCATION "${LIBFTDI1_LIBRARY}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFTDI1_INCLUDE_DIR}")

  # LINK_ONLY keeps libusb's usage requirements out of consumers that include
  # ftdi.h, but still puts libusb on the link line (needed for static libftdi).
  set_property(TARGET LibFTDI1::ftdi1 APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:LibUSB1::usb>")
endif()
