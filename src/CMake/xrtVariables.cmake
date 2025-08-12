# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

if (WIN32)
  set (XRT_INSTALL_DIR .)
  set (XRT_INSTALL_BIN_DIR        ${XRT_INSTALL_DIR})
  set (XRT_INSTALL_UNWRAPPED_DIR  ${XRT_INSTALL_BIN_DIR}/unwrapped)
  set (XRT_INSTALL_INCLUDE_DIR    ${XRT_INSTALL_DIR}/include)
  set (XRT_INSTALL_LIB_DIR        ${XRT_INSTALL_DIR}/lib)
  set (XRT_INSTALL_PLUGIN_LIB_DIR ${XRT_INSTALL_LIB_DIR})
  set (XRT_INSTALL_PYTHON_DIR     ${XRT_INSTALL_DIR}/python)
  set (XRT_INSTALL_CMAKE_DIR      ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
else()
  include (GNUInstallDirs)
  message("-- CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
  message("-- CMAKE_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}")
  message("-- CMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}")
  message("-- CMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}")
  set (XRT_INSTALL_DIR            .)
  set (XRT_INSTALL_BIN_DIR        ${XRT_INSTALL_DIR}/${CMAKE_INSTALL_BINDIR})
  set (XRT_INSTALL_UNWRAPPED_DIR  ${XRT_INSTALL_BIN_DIR}/unwrapped)
  set (XRT_INSTALL_INCLUDE_DIR    ${XRT_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})
  set (XRT_INSTALL_LIB_DIR        ${XRT_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR})
  set (XRT_INSTALL_PLUGIN_LIB_DIR ${XRT_INSTALL_LIB_DIR}/xrt/module)
  set (XRT_INSTALL_PYTHON_DIR     ${XRT_INSTALL_DIR}/python)
  set (XRT_INSTALL_CMAKE_DIR      ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
  set (XRT_INSTALL_PKG_CONFIG_DIR ${XRT_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  set (XRT_BUILD_INSTALL_DIR      ${CMAKE_BINARY_DIR}${CMAKE_INSTALL_PREFIX})
  set (XRT_VALIDATE_DIR           ${XRT_INSTALL_DIR}/test)
  set (XRT_NAMELINK_ONLY NAMELINK_ONLY)
  set (XRT_NAMELINK_SKIP NAMELINK_SKIP)

  # Some install actions are excluded when system install path
  # is used.  In particular, do not install to the unwrapped
  # bin dir and do not install configuration scripts.
  if (CMAKE_INSTALL_PREFIX STREQUAL "/usr"
      OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    set (XRT_SYSTEM_INSTALL 1)
    set (XRT_INSTALL_UNWRAPPED_DIR  ${XRT_INSTALL_BIN_DIR})
  endif()
endif()

