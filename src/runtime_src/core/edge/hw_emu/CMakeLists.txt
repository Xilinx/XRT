# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user")
set(XDP_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../user/plugin/xdp")
set(AIE_HW_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user/aie")
set(AIE_HW_EM_CL_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user/aie/common_layer")

include_directories(
  ${EM_SRC_DIR}
  ${DRM_INCLUDE_DIRS}
  ${COMMON_EM_SRC_DIR}
  ${CMAKE_BINARY_DIR} # includes version.h
  )

file(GLOB EM_SRC_FILES
  "${COMMON_EM_SRC_DIR}/*.h"
  "${COMMON_EM_SRC_DIR}/*.cpp"
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cpp"
  )

file(GLOB XRT_CORE_EDGE_USER_AIE_HW_EM_FILES
  "${AIE_HW_EM_SRC_DIR}/*.h"
  "${AIE_HW_EM_SRC_DIR}/*.cpp"
  "${AIE_HW_EM_SRC_DIR}/*.c"
  "${AIE_HW_EM_CL_SRC_DIR}/*.h"
  "${AIE_HW_EM_CL_SRC_DIR}/*.cpp"
  )

file(GLOB XDP_PLUGIN_HW_EM_FILES
  "${XDP_PLUGIN_DIR}/hw_emu_device_offload.h"
  "${XDP_PLUGIN_DIR}/hw_emu_device_offload.cpp"
  "${XDP_PLUGIN_DIR}/plugin_loader.h"
  "${XDP_PLUGIN_DIR}/plugin_loader.cpp"
  "${XDP_PLUGIN_DIR}/hal_profile.h"
  "${XDP_PLUGIN_DIR}/hal_profile.cpp"
  "${XDP_PLUGIN_DIR}/aie_status.h"
  "${XDP_PLUGIN_DIR}/aie_status.cpp"
  "${XDP_PLUGIN_DIR}/aie_profile.h"
  "${XDP_PLUGIN_DIR}/aie_profile.cpp"
  "${XDP_PLUGIN_DIR}/aie_debug.h"
  "${XDP_PLUGIN_DIR}/aie_debug.cpp"

  )

if (DEFINED XRT_AIE_BUILD)
  set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG -D__HWEM__ ${CMAKE_CXX_FLAGS}")
  add_library(core_edge_user_aie_object_hw_em OBJECT ${XRT_CORE_EDGE_USER_AIE_HW_EM_FILES})
endif()

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0 -D__HWEM__)
add_library(core_edgeuser_plugin_xdp_hw_em_objects OBJECT ${XDP_PLUGIN_HW_EM_FILES})

set(AIE_BUILD_DEFINED "$<BOOL:DEFINED XRT_AIE_BUILD>")
set(VE2_DEFINED "$<BOOL:DEFINED EDGE_VE2>")
set(AIE_TARGET_EXISTS "$<TARGET_EXISTS:core_edge_user_aie_object_hw_em>")
set(VE2_TARGET_EXISTS "$<TARGET_EXISTS:core_edge_ve2_hw_emu_objects>")

add_library(xrt_hwemu SHARED ${EM_SRC_FILES}
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_hw_em_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:core_edge_common_objects>
  $<$<AND:${AIE_BUILD_DEFINED},${AIE_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_user_aie_object_hw_em>>
  $<$<AND:${VE2_DEFINED},${VE2_TARGET_EXISTS}>:$<TARGET_OBJECTS:core_edge_ve2_hw_emu_objects>>
)

set_target_properties(xrt_hwemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xrt_hwemu
    PRIVATE
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    xaiengine
    )
else()
  target_link_libraries(xrt_hwemu
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
    target_link_libraries(xrt_hwemu PRIVATE dfx)
    target_compile_definitions(xrt_hwemu PRIVATE XRT_ENABLE_LIBDFX)
  cmake_policy(POP)
endif()

install (TARGETS xrt_hwemu 
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}
)
