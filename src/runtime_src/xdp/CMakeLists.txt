# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ================================================================
# Set up the include directories used in Linux and Windows for all
# of the plugins and subdirectories.  Also, set the install
# locations for the plugins on Linux and Windows
# ================================================================
if (NOT WIN32)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XRT_SOURCE_DIR}/include/1_2
    )
  set(XDP_PLUGIN_INSTALL_DIR ${XRT_INSTALL_LIB_DIR}/xrt/module)

else()

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${XRT_SOURCE_DIR}/include/1_2
    ${KHRONOS}/include
    )
  set(XDP_PLUGIN_INSTALL_DIR ${XRT_INSTALL_BIN_DIR})

endif()

# Only include xdp in npu, alveo, or legacy xrt components
# For npu, set up different flags used conditionally in
# in subdirectories.
if (XRT_NPU)

  set(XDP_CLIENT_BUILD_CMAKE "yes")
  add_compile_options("-DSKIP_IOCTL")
  add_subdirectory(profile)

elseif(XRT_ALVEO OR XRT_EDGE)

  add_subdirectory(appdebug)
  add_subdirectory(debug)
  add_subdirectory(profile)

endif()
