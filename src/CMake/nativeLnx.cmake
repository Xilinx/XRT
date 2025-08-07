# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH


# --- PkgConfig ---
INCLUDE (FindPkgConfig)

# --- DRM ---
pkg_check_modules(DRM REQUIRED libdrm)
IF(DRM_FOUND)
  MESSAGE(STATUS "Looking for DRM - found at ${DRM_PREFIX} ${DRM_VERSION}")
  INCLUDE_DIRECTORIES(${DRM_INCLUDEDIR})
ELSE(DRM_FOUND)
  MESSAGE(FATAL_ERROR "Looking for DRM - not found")
ENDIF(DRM_FOUND)


# --- OpenCL header files ---
find_package(OpenCL)
if (OPENCL_FOUND)
  MESSAGE(STATUS "Looking for OPENCL - found at ${OPENCL_PREFIX} ${OPENCL_VERSION} ${OPENCL_INCLUDEDIR}")
  INCLUDE_DIRECTORIES(${OPENCL_INCLUDEDIR})
else (OPENCL_FOUND)
  MESSAGE(FATAL_ERROR "Looking for OPENCL - not found")
endif (OPENCL_FOUND)

# --- LSB Release ---
find_program(UNAME uname)

execute_process(
  COMMAND awk -F= "$1==\"ID\" {print $2}" /etc/os-release
  COMMAND tr -d "\""
  COMMAND awk "{print tolower($1)}"
  OUTPUT_VARIABLE LINUX_FLAVOR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (${LINUX_FLAVOR} MATCHES "^centos")
  execute_process(
    COMMAND awk "{print $4}" /etc/redhat-release
    COMMAND tr -d "\""
    OUTPUT_VARIABLE LINUX_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  execute_process(
    COMMAND awk -F= "$1==\"VERSION_ID\" {print $2}" /etc/os-release
    COMMAND tr -d "\""
    OUTPUT_VARIABLE LINUX_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
endif()

execute_process(COMMAND ${UNAME} -r
  OUTPUT_VARIABLE LINUX_KERNEL_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

# Static linking creates and installs static tools and libraries. The
# static libraries have system boost dependencies which must be
# resolved in final target.  The tools (currently xbutil2 and xbmgmt2)
# will be statically linked.  Enabled only for ubuntu.
option(XRT_STATIC_BUILD "Enable static building of XRT" OFF)
if ( (${CMAKE_VERSION} VERSION_GREATER "3.16.0")
    AND (NOT XRT_EDGE)
    AND (${LINUX_FLAVOR} MATCHES "^(Ubuntu)")
    )
  message("-- Enabling static artifacts of XRT")
  set(XRT_STATIC_BUILD ON)
endif()

include(CMake/components.cmake)

# Boost Libraries
include (CMake/boostUtil.cmake)

include_directories(${Boost_INCLUDE_DIRS})
add_compile_options("-DBOOST_LOCALE_HIDE_AUTO_PTR")

# Curses
INCLUDE (FindCurses)
find_package(Curses REQUIRED)

# XRT Variables
include (CMake/xrtVariables.cmake)

# Do not use RPATH when installing into /usr
if (NOT XRT_SYSTEM_INSTALL)
  # Define RPATH for embedding in libraries and executables.  This allows
  # package creation to automatically determine dependencies.
  # RPATH relative to location of binary:
  #  bin/../lib, lib/xrt/module/../.., bin/unwrapped/../../lib
  # Note, that in order to disable RPATH insertion for a specific
  # target (say a static executable), use
  #  set_target_properties(<target> PROPERTIES INSTALL_RPATH "")
  SET(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../..:$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
endif()

install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE
  DESTINATION ${XRT_INSTALL_DIR}/license
  COMPONENT ${XRT_BASE_COMPONENT})
message("-- XRT EA eula files  ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")

# --- Create Version header and JSON file ---
include (CMake/version.cmake)

# --- Cache support
include (CMake/ccache.cmake)

message("-- ${CMAKE_SYSTEM_INFO_FILE} (${LINUX_FLAVOR}) (Kernel ${LINUX_KERNEL_VERSION})")
message("-- Compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_C_COMPILER}")

# --- Lint ---
include (CMake/lint.cmake)

xrt_add_subdirectory(runtime_src)

# For local builds installed to /opt/xilinx/xrt, create a symlink from
# lib -> lib64 on the OS variants where CMAKE_INSTALL_PREFIX is lib64
if (NOT XRT_UPSTREAM
    AND (CMAKE_INSTALL_LIBDIR STREQUAL "lib64")
    AND (CMAKE_INSTALL_PREFIX STREQUAL "/opt/xilinx/xrt"))
  set(src_dir ${XRT_BUILD_INSTALL_DIR}/lib64)
  set(tar_dir ${XRT_BUILD_INSTALL_DIR}/lib)
  set(work_dir ${XRT_BUILD_INSTALL_DIR})
  install(CODE "
    message(STATUS \"Creating install symlink: ${tar_dir} -> ${src_dir}\")
    execute_process(
      COMMAND \${CMAKE_COMMAND} -E create_symlink lib64 lib
      WORKING_DIRECTORY \"${work_dir}\"
    )
  ")
  install(DIRECTORY ${tar_dir}
    DESTINATION ${XRT_INSTALL_DIR}
    COMPONENT ${XRT_BASE_DEV_COMPONENT})
endif()

# --- Python bindings ---
xrt_add_subdirectory(python)

message("-- XRT version: ${XRT_VERSION_STRING}")

# -- CPack
include (CMake/cpackLin.cmake)

if (XRT_ALVEO)
  message("-- XRT Alveo drivers will be bundled with the XRT package")
  set (XRT_DKMS_DRIVER_SRC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/core")
  include (CMake/dkms.cmake)
  include (CMake/dkms-aws.cmake)
  include (CMake/dkms-azure.cmake)
  include (CMake/dkms-container.cmake)
else()
  message("-- Skipping bundling of XRT Alveo drivers with XRT package")
endif()

# ICD loader if for installed with base component
if (XRT_BASE)
  include (CMake/icd.cmake)
endif (XRT_BASE)

# --- Change Log ---
include (CMake/changelog.cmake)

# --- Package Config ---
if (XRT_BASE)
  include (CMake/pkgconfig.cmake)
endif (XRT_BASE)

# --- Coverity Support ---
include (CMake/coverity.cmake)

# --- Find Package Support ---
include (CMake/findpackage.cmake)

set (CTAGS "${XRT_SOURCE_DIR}/runtime_src/tools/scripts/tags.sh")
include (CMake/tags.cmake)
