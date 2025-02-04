# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
if (${XRT_BOOST_VERSION} VERSION_LESS 1.64.0)
  add_compile_options(-DNO_BOOST_PROCESS)
endif()

xrt_add_subdirectory(xbutil2)
if(CMAKE_VERSION VERSION_LESS "3.18.0")
  # CPackDeb.cmake does not support -l{dir} which is required
  # when packaging plugin library libxrt_capture.so 
  message(WARNING "CMake version is less than 3.18.0, build of xbtracer/xbreplay disabled")
else()
  xrt_add_subdirectory(xbtracer)
  xrt_add_subdirectory(xbreplay)
endif()
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  xrt_add_subdirectory(xbmgmt2)
  if(NOT WIN32)
    xrt_add_subdirectory(xbtop)
    if (XRT_ALVEO)
      xrt_add_subdirectory(xbflash2)
    endif()
    xrt_add_subdirectory(nagios)
  endif()
endif()

install (PROGRAMS "./common/xball"
  DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  if (NOT WIN32)
    # Use PROGRAMS to allow for execution. This is recommended in the CMake documentation
    # https://cmake.org/cmake/help/latest/command/install.html#files
    # This is used for the setup script to enable auto completion
    install (PROGRAMS
      ./xbmgmt2/xbmgmt-bash-completion
      ./xbmgmt2/xbmgmt-csh-completion
      ./xbmgmt2/xbmgmt-csh-completion-wrapper
      ./xbutil2/xbutil-bash-completion
      ./xbutil2/xbutil-csh-completion
      ./xbutil2/xbutil-csh-completion-wrapper
      DESTINATION ${XRT_INSTALL_DIR}/share/completions COMPONENT ${XRT_BASE_COMPONENT})
  endif()
endif()
