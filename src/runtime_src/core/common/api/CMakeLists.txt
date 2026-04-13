# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_api_library_objects OBJECT
  context_mgr.cpp
  elf_patcher.cpp
  hw_queue.cpp
  native_profile.cpp
  xrt_bo.cpp
  xrt_device.cpp
  xrt_elf.cpp
  xrt_error.cpp
  xrt_fence.cpp
  xrt_hw_context.cpp
  xrt_ini.cpp
  xrt_ip.cpp
  xrt_kernel.cpp
  xrt_message.cpp
  xrt_module.cpp
  xrt_profile.cpp
  xrt_queue.cpp
  xrt_system.cpp
  xrt_version.cpp
  xrt_xclbin.cpp
  aie/xrt_graph.cpp
  )

target_include_directories(core_common_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/common/elf
  )

# Compile definitions for VE2 XDNA builds to enable run hooks
if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  target_compile_definitions(core_common_api_library_objects
    PRIVATE
    XDP_VE2_BUILD=1
    )
endif()
