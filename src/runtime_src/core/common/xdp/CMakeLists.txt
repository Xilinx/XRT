# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_xdp_profile_objects OBJECT
  profile.cpp
  aie_profile.cpp
  )

target_include_directories(core_common_xdp_profile_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

if (XDP_MINIMAL_BUILD STREQUAL "yes")
  target_compile_definitions(core_common_xdp_profile_objects
    PRIVATE 
    XDP_MINIMAL_BUILD=1
    )
endif()
