#
# Copyright 2021 Xilinx Inc.
# SPDX-License-Identifier: Apache-2.0
#

#
# Findlibffi
# ----------
# Finds the libffi util library
#
# This will define the following variables:
#
# LIBFFI_FOUND - system has libffi
# LIBFFI_INCLUDE_DIRS - the libffi include directory
# LIBFFI_LIBRARIES - the libffi libraries
#

if (LIBFFI_INCLUDE_DIR AND LIBFFI_LIBRARY)
  set(LIBFFI_FIND_QUIETLY true)
endif()

include(FindPackageHandleStandardArgs)

# Find package configuration module
find_package(PkgConfig)

# Find module
pkg_check_modules(PC_LIBFFI QUIET libffi)

# Find include path
find_path(LIBFFI_INCLUDE_DIR ffi.h HINTS ${PC_LIBFFI_INCLUDEDIR} ${PC_LIBFFI_INCLUDE_DIRS})

# Find library
find_library(LIBFFI_LIBRARY NAMES ffi HINTS ${PC_LIBFFI_LIBDIR} ${PC_LIBFFI_LIBRARY_DIRS})

# Define module variables
set(LIBFFI_DEFINITIONS ${PC_LIBFFI_CFLAGS_OTHER})
set(LIBFFI_LIBRARIES ${LIBFFI_LIBRARY})
set(LIBFFI_INCLUDE_DIRS ${LIBFFI_INCLUDE_DIR})

# Define FFI cmake module
find_package_handle_standard_args(LibFFI DEFAULT_MSG LIBFFI_LIBRARY LIBFFI_INCLUDE_DIR)

# Mark cmake module as advanced
mark_as_advanced(LIBFFI_INCLUDE_DIR LIBFFI_LIBRARY)
