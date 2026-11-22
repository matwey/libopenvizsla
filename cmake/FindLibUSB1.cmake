#[=======================================================================[.rst:
FindLibUSB1
-----------

Finds the libusb-1.0 library (also known as libusbx):

.. code-block:: cmake

  find_package(LibUSB1 [<version>] [...])

Imported Targets
^^^^^^^^^^^^^^^^

``LibUSB1::usb``
  Target encapsulating the libusb-1.0 usage requirements.  It is available
  only if the library was found.

Result Variables
^^^^^^^^^^^^^^^^

``LibUSB1_FOUND``
  Boolean indicating whether the (requested version of) libusb-1.0 was found.

``LibUSB1_VERSION``
  Version of the libusb runtime.  It is determined by compiling and running a
  small test program that calls ``libusb_get_version()``.  When cross compiling,
  either set ``CMAKE_CROSSCOMPILING_EMULATOR`` or provide the version manually
  by setting ``LibUSB1_VERSION`` before the ``find_package()`` call.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set to override the search:

``LIBUSB1_INCLUDE_DIR``
  Directory containing ``libusb.h``.

``LIBUSB1_LIBRARY``
  Path to the libusb-1.0 library.

``LIBUDEV_LIBRARY``
  Path to libudev.  It is used only when a link probe shows that the selected
  libusb actually references udev.

Hints
^^^^^

``LibUSB1_ROOT``
  Installation prefix to search first.

On macOS, the system frameworks and Objective-C runtime needed by the static
libusb backend are added explicitly to ``LibUSB1::usb``.

On Linux, the module probes the selected library with a small program that
calls ``libusb_init()`` and ``libusb_exit()``.  A separate thread library and
libudev are added only when that probe shows they are really required, so
builds with udev disabled stay unaffected.  If no dependency combination links,
the package is reported as not found instead of exporting a target that fails
at build time.  Failure of the optional version query is not fatal.

``pkg-config`` is queried only to add hints to the searches above.  The imported
target is built from the paths found by CMake; ``pkg-config`` compiler and
linker flags are deliberately not propagated into it.

Examples
^^^^^^^^

Finding libusb and linking it to a project target:

.. code-block:: cmake

  find_package(LibUSB1 1.0 REQUIRED)
  target_link_libraries(project_target PRIVATE LibUSB1::usb)
#]=======================================================================]

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_LIBUSB1 QUIET libusb-1.0)
endif()

find_path(LIBUSB1_INCLUDE_DIR
  NAMES libusb.h
  HINTS
    ${LibUSB1_ROOT}
    $ENV{LibUSB1_ROOT}
    ${PC_LIBUSB1_INCLUDEDIR}
    ${PC_LIBUSB1_INCLUDE_DIRS}
  PATH_SUFFIXES
    include/libusb-1.0
    libusb-1.0
    include)

find_library(LIBUSB1_LIBRARY
  NAMES usb-1.0 libusb-1.0
  HINTS
    ${LibUSB1_ROOT}
    $ENV{LibUSB1_ROOT}
    ${PC_LIBUSB1_LIBDIR}
    ${PC_LIBUSB1_LIBRARY_DIRS}
  PATH_SUFFIXES
    lib
    lib64)

# The static libusb backend on macOS always needs these system frameworks and
# the Objective-C runtime.  Upstream lists them in Libs.private of
# libusb-1.0.pc; declare them here instead of copying pkg-config output.
set(_libusb1_platform_libraries)
if(APPLE)
  set(_libusb1_platform_libraries
    "-framework CoreFoundation"
    "-framework IOKit"
    "-framework Security"
    "objc")
endif()

set(_libusb1_base_libraries "${LIBUSB1_LIBRARY}")
list(APPEND _libusb1_base_libraries ${_libusb1_platform_libraries})

# On Linux these are optional dependencies of the static backend.  Find them,
# but do not put them on the link line until a link probe proves they are
# needed by the particular libusb binary we selected.
set(_libusb1_thread_libraries)
set(_libusb1_udev_libraries)
if(UNIX AND NOT APPLE)
  find_package(Threads QUIET)
  if(CMAKE_THREAD_LIBS_INIT)
    set(_libusb1_thread_libraries ${CMAKE_THREAD_LIBS_INIT})
  endif()

  if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBUDEV QUIET libudev)
  endif()
  find_library(LIBUDEV_LIBRARY
    NAMES udev
    HINTS
      ${LibUSB1_ROOT}
      $ENV{LibUSB1_ROOT}
      ${PC_LIBUSB1_LIBDIR}
      ${PC_LIBUSB1_LIBRARY_DIRS}
      ${PC_LIBUDEV_LIBDIR}
      ${PC_LIBUDEV_LIBRARY_DIRS}
    PATH_SUFFIXES
      lib
      lib64)
  mark_as_advanced(LIBUDEV_LIBRARY)
  if(LIBUDEV_LIBRARY)
    set(_libusb1_udev_libraries "${LIBUDEV_LIBRARY}")
  endif()
endif()

# The dependency probe initializes the library so the backend object (and its
# udev/thread dependencies) is actually pulled out of a static libusb.
set(_libusb1_link_code [=[
#include <libusb.h>
#include <stddef.h>

int main(void)
{
  libusb_context *ctx = NULL;
  int rc = libusb_init(&ctx);
  if (rc != 0)
    return rc;

  libusb_exit(ctx);
  return 0;
}
]=])

# The version query must not depend on a working USB backend, so it stays
# separate and only calls libusb_get_version().
set(_libusb1_version_code [=[
#include <libusb.h>
#include <stdio.h>

int main(void)
{
  const struct libusb_version *v = libusb_get_version();
  printf("%u.%u.%u", (unsigned)v->major, (unsigned)v->minor, (unsigned)v->micro);
  return 0;
}
]=])

set(_libusb1_version_source
    "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libusb1_version.c")
file(WRITE "${_libusb1_version_source}" "${_libusb1_version_code}")

set(_libusb1_link_libraries ${_libusb1_base_libraries})
set(_libusb1_interface_libraries ${_libusb1_platform_libraries})
set(_libusb1_link_ok FALSE)

# These checks must link an executable; a toolchain that sets STATIC_LIBRARY
# would only archive the probe and silently hide missing backend dependencies.
set(_libusb1_saved_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

if(LIBUSB1_INCLUDE_DIR AND LIBUSB1_LIBRARY)
  include(CheckCSourceCompiles)

  set(_libusb1_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
  set(_libusb1_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
  set(_libusb1_saved_required_quiet "${CMAKE_REQUIRED_QUIET}")
  set(CMAKE_REQUIRED_INCLUDES "${LIBUSB1_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_QUIET TRUE)

  set(_libusb1_probe_sets "BASE")
  if(_libusb1_thread_libraries)
    list(APPEND _libusb1_probe_sets "THREADS")
  endif()
  if(_libusb1_udev_libraries)
    list(APPEND _libusb1_probe_sets "UDEV")
    if(_libusb1_thread_libraries)
      list(APPEND _libusb1_probe_sets "THREADS_UDEV")
    endif()
  endif()

  foreach(_libusb1_probe IN LISTS _libusb1_probe_sets)
    set(_libusb1_probe_libraries ${_libusb1_base_libraries})
    if(_libusb1_probe MATCHES "THREADS")
      list(APPEND _libusb1_probe_libraries ${_libusb1_thread_libraries})
    endif()
    if(_libusb1_probe MATCHES "UDEV")
      list(APPEND _libusb1_probe_libraries ${_libusb1_udev_libraries})
    endif()

    set(CMAKE_REQUIRED_LIBRARIES ${_libusb1_probe_libraries})
    unset(_libusb1_link_probe CACHE)
    check_c_source_compiles("${_libusb1_link_code}" _libusb1_link_probe)

    if(_libusb1_link_probe)
      set(_libusb1_link_ok TRUE)
      set(_libusb1_link_libraries "${_libusb1_probe_libraries}")
      if(_libusb1_probe MATCHES "THREADS")
        list(APPEND _libusb1_interface_libraries ${_libusb1_thread_libraries})
      endif()
      if(_libusb1_probe MATCHES "UDEV")
        list(APPEND _libusb1_interface_libraries ${_libusb1_udev_libraries})
      endif()
      break()
    endif()
  endforeach()

  set(CMAKE_REQUIRED_INCLUDES "${_libusb1_saved_required_includes}")
  set(CMAKE_REQUIRED_LIBRARIES "${_libusb1_saved_required_libraries}")
  set(CMAKE_REQUIRED_QUIET "${_libusb1_saved_required_quiet}")
  unset(_libusb1_saved_required_includes)
  unset(_libusb1_saved_required_libraries)
  unset(_libusb1_saved_required_quiet)

  unset(_libusb1_link_probe CACHE)
  unset(_libusb1_link_probe)
endif()

# libusb has no compile-time version string in its header, so ask the runtime.
# Keep this optional: failing to determine the version must not make the whole
# package unusable when no version was requested.
if(NOT DEFINED LibUSB1_VERSION AND LIBUSB1_INCLUDE_DIR AND LIBUSB1_LIBRARY
   AND (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR))
  unset(_libusb1_run_result CACHE)
  unset(_libusb1_compile_result CACHE)

  try_run(_libusb1_run_result _libusb1_compile_result
    "${CMAKE_BINARY_DIR}"
    "${_libusb1_version_source}"
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES:STRING=${LIBUSB1_INCLUDE_DIR}"
      "-DLINK_LIBRARIES:STRING=${_libusb1_link_libraries}"
    RUN_OUTPUT_VARIABLE _libusb1_version_output)

  if(_libusb1_compile_result AND _libusb1_run_result STREQUAL "0"
     AND _libusb1_version_output MATCHES "^[0-9]+\\.[0-9]+(\\.[0-9]+)?$")
    set(LibUSB1_VERSION "${_libusb1_version_output}")
  endif()

  unset(_libusb1_run_result)
  unset(_libusb1_compile_result)
  unset(_libusb1_version_output)
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_libusb1_saved_try_compile_target_type}")
unset(_libusb1_saved_try_compile_target_type)

file(REMOVE "${_libusb1_version_source}")
unset(_libusb1_version_source)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibUSB1
  REQUIRED_VARS LIBUSB1_LIBRARY LIBUSB1_INCLUDE_DIR
  VERSION_VAR LibUSB1_VERSION)

if(LibUSB1_FOUND AND NOT _libusb1_link_ok)
  # find_package_handle_standard_args() sets both spellings; keep them in sync.
  set(LibUSB1_FOUND FALSE)
  set(LIBUSB1_FOUND FALSE)
  if(LibUSB1_FIND_REQUIRED)
    message(FATAL_ERROR
      "Found libusb at ${LIBUSB1_LIBRARY}, but a link test with its "
      "dependencies failed. Install the backend dependencies (for example "
      "libudev or a separate pthread library) or provide a complete libusb "
      "installation via LIBUSB1_ROOT.")
  elseif(NOT LibUSB1_FIND_QUIETLY)
    message(STATUS "Could NOT find LibUSB1: link test failed for ${LIBUSB1_LIBRARY}")
  endif()
endif()

mark_as_advanced(LIBUSB1_INCLUDE_DIR LIBUSB1_LIBRARY)

if(LibUSB1_FOUND AND NOT TARGET LibUSB1::usb)
  add_library(LibUSB1::usb UNKNOWN IMPORTED)
  set_target_properties(LibUSB1::usb PROPERTIES
    IMPORTED_LOCATION "${LIBUSB1_LIBRARY}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB1_INCLUDE_DIR}")

  if(_libusb1_interface_libraries)
    set_property(TARGET LibUSB1::usb APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES ${_libusb1_interface_libraries})
  endif()
endif()

unset(_libusb1_link_code)
unset(_libusb1_version_code)
unset(_libusb1_link_ok)
unset(_libusb1_link_libraries)
unset(_libusb1_interface_libraries)
unset(_libusb1_base_libraries)
unset(_libusb1_platform_libraries)
unset(_libusb1_thread_libraries)
unset(_libusb1_udev_libraries)
unset(_libusb1_probe_sets)
unset(_libusb1_probe)
unset(_libusb1_probe_libraries)
