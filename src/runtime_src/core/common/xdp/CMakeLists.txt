# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_xdp_profile_objects OBJECT
  profile.cpp
  )

target_include_directories(core_common_xdp_profile_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

# Compile defintions when building npu, alveo, or legacy xrt
# components
if (XRT_NPU)
  
  target_compile_definitions(core_common_xdp_profile_objects
    PRIVATE 
    XDP_CLIENT_BUILD=1
    )
  
elseif (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  
  target_compile_definitions(core_common_xdp_profile_objects
    PRIVATE
    XDP_VE2_BUILD=1
    )

endif()
  

