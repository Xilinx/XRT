# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Profile plugin.  It is currently built
# on Edge, x86, Client, and VE2 platforms that support AIE.
# ====================================================================

file(GLOB AIE_PROFILE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/aie_profile/*.h"
  "${PROFILE_DIR}/plugin/aie_profile/*.cpp"
  "${PROFILE_DIR}/writer/aie_profile/*.h"
  "${PROFILE_DIR}/writer/aie_profile/*.cpp"
)
file(GLOB AIE_PROFILE_UTIL_FILES
  "${PROFILE_DIR}/plugin/aie_base/*"
  "${PROFILE_DIR}/plugin/aie_base/generations/*"
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

if (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/client")

  file(GLOB AIE_PROFILE_IMPL_FILES
    "${IMPL_DIR}/*.h"
    "${IMPL_DIR}/*.cpp"
  )
  xrt_configure_version_file(xdp_aie_profile_plugin SHARED)
  add_library(xdp_aie_profile_plugin SHARED xdp_aie_profile_plugin-version.rc
    ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_IMPL_FILES} ${AIE_DRIVER_COMMON_UTIL_FILES} ${AIE_PROFILE_UTIL_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE XDP_CLIENT_BUILD=1 -DXAIE_FEATURE_MSVC)
  target_include_directories(xdp_aie_profile_plugin PRIVATE ${AIERT_DIR}/include)
  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    RUNTIME DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT} ${XRT_NAMELINK_SKIP}
  )

elseif (DEFINED XRT_AIE_BUILD AND (NOT XRT_EDGE))
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/x86")

  file(GLOB AIE_PROFILE_IMPL_FILES
    "${IMPL_DIR}/*.h"
    "${IMPL_DIR}/*.cpp"
  )
  xrt_configure_version_file(xdp_aie_profile_plugin SHARED)
  add_library(xdp_aie_profile_plugin SHARED xdp_aie_profile_plugin-version.rc
    ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_IMPL_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil)
  target_compile_definitions(xdp_aie_profile_plugin PRIVATE XRT_X86_BUILD=1)

  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    RUNTIME DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT} ${XRT_NAMELINK_SKIP}
  )

else()

  if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
    set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/ve2")

    file(GLOB AIE_PROFILE_IMPL_FILES
      "${IMPL_DIR}/*.h"
      "${IMPL_DIR}/*.cpp"
    )
    add_library(xdp_aie_profile_plugin_xdna SHARED ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_IMPL_FILES} ${AIE_PROFILE_UTIL_FILES} ${AIE_PROFILE_CONFIG_FILES})
    add_dependencies(xdp_aie_profile_plugin_xdna xdp_core xrt_coreutil)
    target_link_libraries(xdp_aie_profile_plugin_xdna PRIVATE xdp_core xrt_coreutil xaiengine)
    target_compile_definitions(xdp_aie_profile_plugin_xdna PRIVATE XDP_VE2_BUILD=1 FAL_LINUX="on")
    target_include_directories(xdp_aie_profile_plugin_xdna PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(xdp_aie_profile_plugin_xdna PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

    install (TARGETS xdp_aie_profile_plugin_xdna
      RUNTIME DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
      LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT} ${XRT_NAMELINK_SKIP}
    )
  endif()

  if (DEFINED XRT_AIE_BUILD AND XRT_EDGE)
    set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_profile/edge")

    file(GLOB AIE_PROFILE_IMPL_FILES
      "${IMPL_DIR}/*.h"
      "${IMPL_DIR}/*.cpp"
    )
    add_library(xdp_aie_profile_plugin SHARED ${AIE_PROFILE_PLUGIN_FILES} ${AIE_PROFILE_IMPL_FILES} ${AIE_PROFILE_UTIL_FILES} ${AIE_PROFILE_CONFIG_FILES})
    add_dependencies(xdp_aie_profile_plugin xdp_core xrt_coreutil)
    target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core xrt_coreutil xaiengine)
    if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
      target_compile_definitions(xdp_aie_profile_plugin PRIVATE XDP_VE2_ZOCL_BUILD=1 FAL_LINUX="on")
    else()
      target_compile_definitions(xdp_aie_profile_plugin PRIVATE FAL_LINUX="on")
    endif()
    set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

    install (TARGETS xdp_aie_profile_plugin
      RUNTIME DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
      LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT} ${XRT_NAMELINK_SKIP}
    )
  endif()

# Else, on edge-aarch64 don't build at all

endif()
