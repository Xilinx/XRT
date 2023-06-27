# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

# xrt_add_subdirectory subdir [bindir] [exclude_from_all] [system]
# See CMake: add_subdirectory
#
# This function overrides add_subdirectory to exclude directories
# specified in CMake list XRT_EXCLUDE_SUB_DIRECTORY.
#
# The specified exclude direectories must be relative to the XRT root
# project directory, e.g. src/runtime_src/xocl, tests/validate
#
# Exclusion facilitates projects using XRT as a submodule while
# building only a select subset of XRT.
function(xrt_add_subdirectory subdir)
  # convert ../blah to absolute path
  get_filename_component(path ${CMAKE_CURRENT_SOURCE_DIR}/${subdir} ABSOLUTE)

  # exclusion is specified relative to XRT root directory
  get_filename_component(xrtroot ${XRT_SOURCE_DIR} DIRECTORY)
  file(RELATIVE_PATH relpath ${xrtroot} ${path})
  if (${relpath} IN_LIST XRT_EXCLUDE_SUB_DIRECTORY)
    message("-- xrt_add_subdirectory excludes ${subdir}")
  elseif (EXISTS ${path})                     
    message("-- add_subdirectory(${ARGV})")
    add_subdirectory(${ARGV})
  else ()
    message("-- xrt_add_subdirectory ${subdir} does not exist")
  endif()
endfunction()

# Override CMake include to conditionally exclude include dirs
# Same behavior as xrt_add_subdirectory
function(xrt_include filename)
  # convert ../blah to absolute path
  get_filename_component(path ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ABSOLUTE)

  # exclusion is specified relative to XRT root directory
  get_filename_component(xrtroot ${XRT_SOURCE_DIR} DIRECTORY)
  file(RELATIVE_PATH relpath ${xrtroot} ${path})
  if (${relpath} IN_LIST XRT_EXCLUDE_INCLUDE_FILE)
    message("-- xrt_include excludes ${filename}")
  else()
    message("-- include(${ARGV})")
    include(${ARGV})
  endif()
endfunction()
