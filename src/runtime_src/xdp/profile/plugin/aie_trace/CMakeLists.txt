# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Trace plugin.  It is currently built on x86,
# Edge-Versal systems, client and ve2 but not Edge-aarch64.  It also has a dependency
# on the hardware shim
# ====================================================================
if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace/ve2")
  set(OFFLOAD_DIR "${PROFILE_DIR}/device/aie_trace/ve2")
  set(DEVICE_DIR "${PROFILE_DIR}/device/hal_device")
elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace/client")
  set(OFFLOAD_DIR "${PROFILE_DIR}/device/aie_trace/client")
  set(DEVICE_DIR "${PROFILE_DIR}/device/client_device")
elseif (${XRT_NATIVE_BUILD} STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace/x86")
  set(OFFLOAD_DIR "${PROFILE_DIR}/device/aie_trace")
  set(DEVICE_DIR "${PROFILE_DIR}/device/hal_device")
elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace/edge")
  set(OFFLOAD_DIR "${PROFILE_DIR}/device/aie_trace")
  set(DEVICE_DIR "${PROFILE_DIR}/device/hal_device")
endif()

file(GLOB AIE_TRACE_PLUGIN_FILES
  "${OFFLOAD_DIR}/*.h"
  "${OFFLOAD_DIR}/*.cpp"
  "${DEVICE_DIR}/*.h"
  "${DEVICE_DIR}/*.cpp"
  "${PROFILE_DIR}/plugin/aie_trace/*.h"
  "${PROFILE_DIR}/plugin/aie_trace/*.cpp"
  "${PROFILE_DIR}/writer/aie_trace/*.h"
  "${PROFILE_DIR}/writer/aie_trace/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)
file(GLOB AIE_TRACE_UTIL_FILES
  "${PROFILE_DIR}/plugin/aie_trace/util/aie_trace_util.h"
  "${PROFILE_DIR}/plugin/aie_trace/util/aie_trace_util.cpp"
)
file(GLOB AIE_TRACE_CONFIG_FILES
  "${PROFILE_DIR}/plugin/aie_trace/util/aie_trace_config.h"
  "${PROFILE_DIR}/plugin/aie_trace/util/aie_trace_config.cpp"
)
file(GLOB AIE_DRIVER_COMMON_UTIL_FILES
  "${PROFILE_DIR}/device/common/*.h"
  "${PROFILE_DIR}/device/common/*.cpp"
)

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES} ${AIE_TRACE_UTIL_FILES} ${AIE_TRACE_CONFIG_FILES})
  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE XDP_VE2_BUILD=1 FAL_LINUX="on")
  target_include_directories(xdp_aie_trace_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES} ${AIE_TRACE_UTIL_FILES} ${AIE_DRIVER_COMMON_UTIL_FILES})
  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE XDP_CLIENT_BUILD=1 -DXAIE_FEATURE_MSVC)
  target_include_directories(xdp_aie_trace_plugin PRIVATE ${AIERT_DIR}/include)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES})
  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_coreutil)
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE XRT_X86_BUILD=1)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES} ${AIE_TRACE_UTIL_FILES} ${AIE_TRACE_CONFIG_FILES})
  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE FAL_LINUX="on")
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

# Else, on edge-aarch64 don't build at all

endif()


