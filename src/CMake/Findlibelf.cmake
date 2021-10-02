#
# Copyright 2021 Xilinx Inc.
# SPDX-License-Identifier: Apache-2.0
#

#
# Findlibelf
# ----------
# Finds the libelf util library
#
# This will define the following variables:
#
# LIBELF_FOUND - system has libelf
# LIBELF_INCLUDE_DIRS - the libelf include directory
# LIBELF_LIBRARIES - the libelf libraries
#

if (LIBELF_INCLUDE_DIR AND LIBELF_LIBRARY)
  set(LIBELF_FIND_QUIETLY true)
endif()

find_path(LIBELF_INCLUDE_DIR libelf.h HINTS ${LIBELF_PREFIX}/include)

find_library(LIBELF_LIBRARY NAMES elf HINTS ${LIBELF_PREFIX}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBELF DEFAULT_MSG LIBELF_LIBRARY LIBELF_INCLUDE_DIR)

mark_as_advanced(LIBELF_INCLUDE_DIR  LIBELF_LIBRARY)
