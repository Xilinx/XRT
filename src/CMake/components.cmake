# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

# Custom variables imported by this CMake stub which should be defined by parent CMake:
# LINUX_FLAVOR

if (NOT WIN32)
  if (${LINUX_FLAVOR} MATCHES "^(ubuntu|debian)")
    set (XRT_DEV_COMPONENT_SUFFIX "dev")
  elseif (${LINUX_FLAVOR} MATCHES "^(rhel|centos)")
    set (XRT_DEV_COMPONENT_SUFFIX "devel")
  endif()
else()
  set(LINUX_FLAVOR "none")
endif()

# The default XRT build is legacy
if (NOT XRT_BASE AND NOT XRT_NPU AND NOT XRT_ALVEO)
  message("-- Defaulting to legacy XRT build")
  set(XRT_XRT 1)
endif()

# Enable development package by specifying development component name
# If XRT_{PKG}_DEV_COMPONENT is same XRT_{PKG}_COMPONENT then only
# that package is created with both development and run-time content.
set (XRT_COMPONENT "xrt")
set (XRT_DEV_COMPONENT "xrt")
set (XRT_BASE_COMPONENT "base")
set (XRT_BASE_DEV_COMPONENT "base")
set (XRT_ALVEO_COMPONENT "alveo")
set (XRT_ALVEO_DEV_COMPONENT "alveo")
set (XRT_NPU_COMPONENT "npu")
set (XRT_NPU_DEV_COMPONENT "npu")

if (NOT WIN32 AND ${LINUX_FLAVOR} MATCHES "^(ubuntu|debian|rhel|centos)")
  if (${LINUX_FLAVOR} MATCHES "^(ubuntu|debian)")
    set (XRT_DEV_COMPONENT_SUFFIX "dev")
  elseif (${LINUX_FLAVOR} MATCHES "^(rhel|centos)")
    set (XRT_DEV_COMPONENT_SUFFIX "devel")
  endif()

  set (XRT_BASE_DEV_COMPONENT "base-${XRT_DEV_COMPONENT_SUFFIX}")
  set (XRT_ALVEO_DEV_COMPONENT "alveo-${XRT_DEV_COMPONENT_SUFFIX}")
  set (XRT_NPU_DEV_COMPONENT "npu-${XRT_DEV_COMPONENT_SUFFIX}")
endif()

# BASE builds a deployment and development package of everything
# enabled by XRT_BASE
if (XRT_BASE)
  # For the time being, dump everything into base that has not
  # been explicitly marked for base
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "base")
  set (XRT_COMPONENT ${XRT_BASE_COMPONENT})
  set (XRT_DEV_COMPONENT ${XRT_BASE_DEV_COMPONENT})
endif(XRT_BASE)  

# NPU builds one NPU package for both deployment and development
# for everything enabled by XRT_NPU and XRT_BASE
if (XRT_NPU)
  set (XRT_BASE 1)
  set (XRT_BASE_COMPONENT "npu")
  set (XRT_BASE_DEV_COMPONENT "npu")
  set (XRT_NPU_COMPONENT "npu")
  set (XRT_NPU_DEV_COMPONENT "npu")

  # For the time being, dump everything into npu that has not
  # been explicitly marked alveo or npu
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "npu")
  set (XRT_COMPONENT ${XRT_NPU_COMPONENT})
  set (XRT_DEV_COMPONENT ${XRT_NPU_DEV_COMPONENT})
endif(XRT_NPU)

# Alveo builds one Alveo package for both deployment and development
# for everything enabled by XRT_ALVEO and XRT_BASE
if (XRT_ALVEO)
  set (XRT_BASE 1)

  set (XRT_BASE_COMPONENT "alveo")
  set (XRT_BASE_DEV_COMPONENT "alveo")
  set (XRT_ALVEO_COMPONENT "alveo")
  set (XRT_ALVEO_DEV_COMPONENT "alveo")

  # For the time being, dump everything into alveo that has not
  # been explicitly marked alveo or npu
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "alveo")
  set (XRT_COMPONENT ${XRT_ALVEO_COMPONENT})
  set (XRT_DEV_COMPONENT ${XRT_ALVEO_DEV_COMPONENT})
endif(XRT_ALVEO)

# Legacy, build one XRT package for both deployment and development
# Include everything enabled by XRT_BASE and XRT_ALVEO
if (XRT_XRT)
  set (XRT_BASE 1)
  set (XRT_ALVEO 1)
  set (XRT_NPU 0)
  
  set (XRT_BASE_COMPONENT "xrt")
  set (XRT_BASE_DEV_COMPONENT "xrt")
  set (XRT_ALVEO_COMPONENT "xrt")
  set (XRT_ALVEO_DEV_COMPONENT "xrt")
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "xrt")
endif(XRT_XRT)

