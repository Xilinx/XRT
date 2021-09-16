#
# Copyright 2021 Xilinx Inc.
# SPDX-License-Identifier: Apache-2.0
#

#
# Findlibdw
# ----------
# Finds the libdw util library
#
# This will define the following variables:
#
# LIBDW_FOUND - system has libdw
# LIBDW_INCLUDE_DIRS - the libdw include directory
# LIBDW_LIBRARIES - the libdw libraries
#

if (LIBDW_INCLUDE_DIR AND LIBDW_LIBRARY)
  set(LIBDW_FIND_QUIETLY true)
endif()

find_path(LIBDW_INCLUDE_DIR elfutils/libdw.h HINTS ${LIBDW_PREFIX}/include)

find_library(LIBDW_LIBRARY NAMES dw HINTS ${LIBDW_PREFIX}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBDW DEFAULT_MSG LIBDW_LIBRARY LIBDW_INCLUDE_DIR)

mark_as_advanced(LIBDW_INCLUDE_DIR  LIBDW_LIBRARY)
