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
endif(NOT WIN32)

# Default component name for any install() command without the COMPONENT argument
# The default component is the xrt run-time component, if XRT_DEV_COMPONENT is
# set to something different then a development component will be created with
# link libraries and header which are then excluded from runtime component
set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "xrt")
  
# Enable development package by specifying development component name
# If XRT_DEV_COMPONENT is same DEFAULT_COMPONENT then only that package
# is created with both development and run-time content.
#set (XRT_DEV_COMPONENT "xrt-dev")
set (XRT_COMPONENT "xrt")
set (XRT_DEV_COMPONENT "xrt")

set (XRT_BASE_COMPONENT "base")
set (XRT_BASE_DEV_COMPONENT "base-${XRT_DEV_COMPONENT_SUFFIX}")

# For NPU builds the defalt component is "npu".
if (XRT_NPU)
  set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "npu")
  set (XRT_COMPONENT "npu")
  set (XRT_DEV_COMPONENT "npu")
endif(XRT_NPU)
