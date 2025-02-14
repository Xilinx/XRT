# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Debug plugin.  It is currently built
# on Edge, Client, and VE2 platforms that support AIE.
# Note it is not built or supported on x86.
# ====================================================================

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_debug/ve2")
elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_debug/client")
elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_debug/edge")
endif()

file(GLOB AIE_DEBUG_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/aie_debug/*.h"
  "${PROFILE_DIR}/plugin/aie_debug/*.cpp"
  "${PROFILE_DIR}/plugin/aie_debug/generations/*.h"
  "${PROFILE_DIR}/writer/aie_debug/*.h"
  "${PROFILE_DIR}/writer/aie_debug/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)

file(GLOB AIE_DRIVER_COMMON_UTIL_FILES
  "${PROFILE_DIR}/device/common/*.h"
  "${PROFILE_DIR}/device/common/*.cpp"
)

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_debug_plugin MODULE ${AIE_DEBUG_PLUGIN_FILES})
  add_dependencies(xdp_aie_debug_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_debug_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_debug_plugin PRIVATE FAL_LINUX="on" XDP_VE2_BUILD=1)
  target_include_directories(xdp_aie_debug_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
  
  install (TARGETS xdp_aie_debug_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
  )

elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_debug_plugin MODULE ${AIE_DEBUG_PLUGIN_FILES} ${AIE_DRIVER_COMMON_UTIL_FILES})
  add_dependencies(xdp_aie_debug_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_debug_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_debug_plugin PRIVATE XDP_CLIENT_BUILD=1 -DXAIE_FEATURE_MSVC)
  target_include_directories(xdp_aie_debug_plugin PRIVATE ${AIERT_DIR}/include)
  set_target_properties(xdp_aie_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_debug_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  add_library(xdp_aie_debug_plugin MODULE ${AIE_DEBUG_PLUGIN_FILES})
  add_dependencies(xdp_aie_debug_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_debug_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_debug_plugin PRIVATE FAL_LINUX="on")
  set_target_properties(xdp_aie_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_debug_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
  )

# Else, on edge-aarch64 don't build at all

endif()