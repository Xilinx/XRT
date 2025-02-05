# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

# Custom variables imported by this CMake stub which should be defined
# by parent CMake: LINUX_FLAVOR

# The default XRT build is legacy
if (NOT XRT_BASE AND NOT XRT_NPU AND NOT XRT_ALVEO)
  message("-- Defaulting to legacy XRT build")
  set(XRT_XRT 1)
endif()

if (NOT WIN32)
  if (${LINUX_FLAVOR} MATCHES "^(ubuntu|debian)")
    set (XRT_DEV_COMPONENT_SUFFIX "-dev")
  elseif (${LINUX_FLAVOR} MATCHES "^(rhel|centos|fedora)")
    set (XRT_DEV_COMPONENT_SUFFIX "-devel")
  endif()
endif()

# NSIS packager cannot handle '-' in component names
if (WIN32)
  set (XRT_DEV_COMPONENT_SUFFIX "_dev")
endif()   

# Enable development package by specifying development component name
# If XRT_{PKG}_DEV_COMPONENT is same XRT_{PKG}_COMPONENT then only
# that package is created with both development and run-time content.
# XRT currently supports building four packages: base, npu, alveo, and
# legacy xrt. The package built by XRT is controlled through CMake
# variables:
# - XRT_BASE:
#    Builds the base package, which contains everything shared by
#    other packages.
# - XRT_NPU:
#    Builds the npu package with content specific to NPU.
# - XRT_ALVEO:
#    Builds the alveo package with content specific to Alveo.
# - XRT_XRT: (default when no other variables are set)
#    Builds the legacy XRT package.
#
# The packages are created by populating CMake components controlled
# through following symbolic variables, which are set to either
# base, npu, alveo, or xrt depending on which package is built.
# - XRT_BASE_COMPONENT:
#    Deployment component for XRT base content.  The base component
#    has content that is required by npu, alveo, and xrt packages.
# - XRT_BASE_DEV_COMPONENT:
#    Development component for XRT base content.
# - XRT_COMPONENT:
#    Deployment component for XRT npu, alveo, or legacy xrt packages.
#    This component is dependent on the base component.
# - XRT_DEV_COMPONENT:
#    Development compoment for XRT npu, alveo, or legacy xrt packages.
#
# The XRT_COMPONENT and XRT_DEV_COMPONENT are used to control content
# that is installed for multiple exclusive targets (npu or alveo) and
# used to indicate packaging for legacy XRT build (XRT_XRT).  These
# symbolic component names are explicitly named depending on the
# package being built.

# BASE builds a deployment and development package of everything
# enabled by XRT_BASE.
if (XRT_BASE)
  # For the time being, dump everything into base that has not
  # been explicitly marked for base
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "base")
  set (XRT_BASE_COMPONENT "base")
  set (XRT_BASE_DEV_COMPONENT "base${XRT_DEV_COMPONENT_SUFFIX}")

  # Tempoary fix for cpackLin conditionally adding dependencies for
  # legacy XRT when XRT_DEV_COMPONENT equals "xrt".  We don't want the
  # dependencies in case of base package.
  set (XRT_DEV_COMPONENT "dummy")
endif(XRT_BASE)  

# NPU builds one NPU package for both deployment and development
# for everything enabled by XRT_NPU and XRT_BASE
if (XRT_NPU)
  set (XRT_BASE 1)

  # For the time being, dump everything into npu that has not
  # been explicitly marked alveo or npu
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "npu")
  set (XRT_COMPONENT "npu")
  set (XRT_DEV_COMPONENT "npu${XRT_DEV_COMPONENT_SUFFIX}")
  set (XRT_BASE_COMPONENT "base")
  set (XRT_BASE_DEV_COMPONENT "base${XRT_DEV_COMPONENT_SUFFIX}")
endif(XRT_NPU)

# Alveo builds one Alveo package for both deployment and development
# for everything enabled by XRT_ALVEO and XRT_BASE
if (XRT_ALVEO)
  set (XRT_BASE 1)

  # For the time being, dump everything into alveo that has not
  # been explicitly marked alveo or npu
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "alveo")
  set (XRT_COMPONENT "alveo")
  set (XRT_DEV_COMPONENT "alveo")
  set (XRT_BASE_COMPONENT ${XRT_COMPONENT})
  set (XRT_BASE_DEV_COMPONENT ${XRT_DEV_COMPONENT})
endif(XRT_ALVEO)

# Legacy, build one XRT package for both deployment and development
# Include everything enabled by XRT_BASE and XRT_ALVEO
if (XRT_XRT)
  set (XRT_BASE 1)
  set (XRT_ALVEO 1)
  set (XRT_NPU 0)
  
  # The default install component is XRT
  # Combine XRT_DEV_COMPONENT with XRT_COMPONENT
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "xrt")
  set (XRT_COMPONENT "xrt")
  set (XRT_DEV_COMPONENT "xrt")
  set (XRT_BASE_COMPONENT ${XRT_COMPONENT})
  set (XRT_BASE_DEV_COMPONENT ${XRT_DEV_COMPONENT})
endif(XRT_XRT)

