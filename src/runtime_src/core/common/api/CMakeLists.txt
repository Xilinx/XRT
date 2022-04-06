# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_api_library_objects OBJECT
  context_mgr.cpp
  exec.cpp
  kds.cpp
  native_profile.cpp
  sws.cpp
  xrt_bo.cpp
  xrt_device.cpp
  xrt_enqueue.cpp
  xrt_error.cpp
  xrt_ini.cpp
  xrt_ip.cpp
  xrt_kernel.cpp
  xrt_message.cpp
  xrt_pipeline.cpp
  xrt_profile.cpp
  xrt_queue.cpp
  xrt_system.cpp
  xrt_xclbin.cpp
  aie/xrt_graph.cpp
  )

target_include_directories(core_common_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )
