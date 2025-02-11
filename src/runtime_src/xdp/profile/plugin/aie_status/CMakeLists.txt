# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Status plugin.  It is currently built on
# Edge and VE2 platforms that support AIE.
# ====================================================================

file(GLOB AIE_STATUS_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/aie_status/*.h"
  "${PROFILE_DIR}/plugin/aie_status/*.cpp"
  "${PROFILE_DIR}/writer/aie_status/*.h"
  "${PROFILE_DIR}/writer/aie_status/*.cpp"
)

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_aie_status_plugin MODULE ${AIE_STATUS_PLUGIN_FILES})
  add_dependencies(xdp_aie_status_plugin xdp_core)
  target_link_libraries(xdp_aie_status_plugin PRIVATE xdp_core xaiengine)
  target_compile_definitions(xdp_aie_status_plugin PRIVATE XDP_VE2_BUILD=1 FAL_LINUX="on")
  target_include_directories(xdp_aie_status_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
  set_target_properties(xdp_aie_status_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_status_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  add_library(xdp_aie_status_plugin MODULE ${AIE_STATUS_PLUGIN_FILES})
  add_dependencies(xdp_aie_status_plugin xdp_core)
  target_link_libraries(xdp_aie_status_plugin PRIVATE xdp_core xaiengine)
  target_compile_definitions(xdp_aie_status_plugin PRIVATE FAL_LINUX="on")

  set_target_properties(xdp_aie_status_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_status_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

endif()
