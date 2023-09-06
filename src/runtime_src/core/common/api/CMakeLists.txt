# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_api_library_objects OBJECT
  context_mgr.cpp
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
  )

if (DEFINED XRT_AIE_BUILD)
  message("-- enabling api aie build")
  target_sources(core_common_api_library_objects
    PRIVATE
    aie/xrt_graph.cpp)
endif()

target_include_directories(core_common_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/common/elf
  )
