# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Profile plugin.  It is currently built
# on Edge, x86, Client, and VE2 platforms that support AIE.
# ====================================================================

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/ve2")
elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/client")
elseif (${XRT_NATIVE_BUILD} STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/x86")
elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/edge")
endif()

file(GLOB AIE_PROFILE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/aie_profile/*.h"
  "${PROFILE_DIR}/plugin/aie_profile/*.cpp"
  "${PROFILE_DIR}/writer/aie_profile/*.h"
  "${PROFILE_DIR}/writer/aie_profile/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)
file(GLOB AIE_PROFILE_UTIL_FILES
  "${PROFILE_DIR}/plugin/aie_profile/util/aie_profile_util.h"
  "${PROFILE_DIR}/plugin/aie_profile/util/aie_profile_util.cpp"
)
file(GLOB AIE_PROFILE_CONFIG_FILES
  "${PROFILE_DIR}/plugin/aie_profile/util/aie_profile_config.h"
  "${PROFILE_DIR}/plugin/aie_profile/util/aie_profile_config.cpp"
)
file(GLOB AIE_DRIVER_COMMON_UTIL_FILES
  "${PROFILE_DIR}/device/common/*.h"
  "${PROFILE_DIR}/device/common/*.cpp"
)

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_profile_plugin MODULE ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_UTIL_FILES} ${AIE_PROFILE_CONFIG_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE XDP_VE2_BUILD=1 FAL_LINUX="on")
  target_include_directories(xdp_aie_profile_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_profile_plugin MODULE ${AIE_PROFILE_PLUGIN_FILES} ${AIE_DRIVER_COMMON_UTIL_FILES} ${AIE_PROFILE_UTIL_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE XDP_CLIENT_BUILD=1 -DXAIE_FEATURE_MSVC)
  target_include_directories(xdp_aie_profile_plugin PRIVATE ${AIERT_DIR}/include)
  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_library(xdp_aie_profile_plugin MODULE ${AIE_PROFILE_PLUGIN_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE XRT_X86_BUILD=1)

  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  add_library(xdp_aie_profile_plugin MODULE ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_UTIL_FILES} ${AIE_PROFILE_CONFIG_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE FAL_LINUX="on")
  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

# Else, on edge-aarch64 don't build at all

endif()
