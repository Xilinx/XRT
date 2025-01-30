# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# This cmake file is for embedded system. Only support cross compile aarch64
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH

INCLUDE (FindPkgConfig)

# DRM
if (NOT DEFINED CROSS_COMPILE)
  pkg_check_modules(DRM REQUIRED libdrm)
  IF(DRM_FOUND)
    MESSAGE(STATUS "Looking for DRM - found at ${DRM_PREFIX} ${DRM_VERSION}")
    INCLUDE_DIRECTORIES(${DRM_INCLUDEDIR})
  ELSE(DRM_FOUND)
    MESSAGE(FATAL_ERROR "Looking for DRM - not found")
  ENDIF(DRM_FOUND)
endif()

# OpenCL header files
find_package(OpenCL)
IF(OPENCL_FOUND)
  MESSAGE(STATUS "Looking for OPENCL - found at ${OPENCL_PREFIX} ${OPENCL_VERSION} ${OPENCL_INCLUDEDIR}")
  INCLUDE_DIRECTORIES(${OPENCL_INCLUDEDIR})
ELSE(OPENCL_FOUND)
  MESSAGE(FATAL_ERROR "Looking for OPENCL - not found")
ENDIF(OPENCL_FOUND)

find_package(Git)

IF(GIT_FOUND)
  message("git found: ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

set(LINUX_FLAVOR ${CMAKE_SYSTEM_NAME})
set(LINUX_KERNEL_VERSION ${CMAKE_SYSTEM_VERSION})

# Set up what components of XRT to build
# Indicate that we are building for edge
include(CMake/components.cmake)
set(XRT_EDGE 1)

# --- Boost Libraries ---
include (CMake/boostUtil.cmake)

INCLUDE (FindCurses)
find_package(Curses REQUIRED)

# --- XRT Variables ---
include (CMake/xrtVariables.cmake)

#Setting RPATH variable for cross compilation
if (DEFINED CROSS_COMPILE)
  set(CMAKE_INSTALL_RPATH "${sysroot}/usr/lib:${sysroot}/lib:${sysroot}/usr/lib/aarch64-linux-gnu")
endif()

add_compile_options("-DXRT_EDGE")

# Let yocto handle license files in the standard way

include (CMake/version.cmake)

include (CMake/ccache.cmake)

message("-- ${CMAKE_SYSTEM_INFO_FILE} (${LINUX_FLAVOR}) (Kernel ${LINUX_KERNEL_VERSION})")
message("-- Compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_C_COMPILER}")

# --- Lint ---
include (CMake/lint.cmake)

add_subdirectory(runtime_src)

message("-- XRT version: ${XRT_VERSION_STRING}")

message("-- Preparing XRT pkg-config")
set(XRT_PKG_CONFIG_DIR "/usr/lib/pkgconfig")

configure_file (
  ${XRT_SOURCE_DIR}/CMake/config/xrt-edge.pc.in
  xrt.pc
  @ONLY
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/xrt.pc
  DESTINATION ${XRT_PKG_CONFIG_DIR}
  COMPONENT ${XRT_DEV_COMPONENT}
  )

# --- Find Package Support ---
include (CMake/findpackage.cmake)

if (DEFINED CROSS_COMPILE)
  set (LINUX_FLAVOR ${flavor})
  set (LINUX_VERSION ${version})
  include (CMake/cpackLin.cmake)
endif()

if (DEFINED ENV{DKMS_FLOW})
  set (XRT_DKMS_DRIVER_SRC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/core")
  include (CMake/dkms-edge.cmake)
endif()
