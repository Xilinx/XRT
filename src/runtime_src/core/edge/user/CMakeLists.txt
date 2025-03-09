# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
include_directories(
  ${DRM_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR} # include gen/version.h
  )

add_subdirectory(plugin/xdp)

if (DEFINED XRT_AIE_BUILD)
  file(GLOB XRT_CORE_EDGE_USER_AIE_FILES
    "aie/*.h"
    "aie/*.cpp"
    "aie/*.c"
    "aie/common_layer/*.h"
    "aie/common_layer/*.cpp"
  )

  set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG ${CMAKE_CXX_FLAGS}")
  add_library(core_edge_user_aie_object OBJECT ${XRT_CORE_EDGE_USER_AIE_FILES})
endif()

file(GLOB XRT_EDGE_USER_FILES "*.cpp")

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

set(AIE_BUILD_DEFINED "$<BOOL:DEFINED XRT_AIE_BUILD>")
set(VE2_DEFINED "$<BOOL:DEFINED EDGE_VE2>")
set(AIE_TARGET_EXISTS "$<TARGET_EXISTS:core_edge_user_aie_object>")
set(VE2_TARGET_EXISTS "$<TARGET_EXISTS:core_edge_ve2_objects>")

add_library(xrt_core SHARED ${XRT_EDGE_USER_FILES}
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:core_edge_common_objects>
  $<$<AND:${AIE_BUILD_DEFINED},${AIE_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_user_aie_object>>
  $<$<AND:${VE2_DEFINED},${VE2_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_ve2_objects>>
  )

add_library(xrt_core_static STATIC ${XRT_EDGE_USER_FILES}
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_no_dl_load_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:core_edge_common_objects>
  $<$<AND:${AIE_BUILD_DEFINED},${AIE_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_user_aie_object>>
  $<$<AND:${VE2_DEFINED},${VE2_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_ve2_objects>>
  )

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xrt_core
    PRIVATE
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    xaiengine
    )
else()
  target_link_libraries(xrt_core
    PRIVATE
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    )

endif()

if (DEFINED XRT_LIBDFX)
  cmake_policy(PUSH)
    #Setting this policy makes linking libraries simple
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(xrt_core PRIVATE dfx)
    target_compile_definitions(xrt_core PRIVATE XRT_ENABLE_LIBDFX)
  cmake_policy(POP)
endif()

install (TARGETS xrt_core 
  EXPORT xrt-targets 
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}
)
