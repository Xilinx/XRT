# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if (${XRT_BOOST_VERSION} VERSION_LESS 1.64.0)
  add_compile_options(-DNO_BOOST_PROCESS)
endif()

add_subdirectory(xbutil2)
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(xbmgmt2)
endif()

install (PROGRAMS "./common/xball" DESTINATION ${XRT_INSTALL_BIN_DIR})
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  if (NOT WIN32)
    # Use PROGRAMGS to allow for execution. This is recommended in the CMake documentation
    # https://cmake.org/cmake/help/latest/command/install.html#files
    install (PROGRAMS "./xbutil2/bash-completion/xbutil" DESTINATION "/usr/share/bash-completion/completions")
  endif()
endif()