# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Devices, Inc.  All rights reserved.
#

# ==============================================================================
# This builds the FlexML Timeline plugin.  It is currently built on Windows Only.
# ==============================================================================

if (XDP_MINIMAL_BUILD STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/flexml_timeline/clientDev")
endif()

file(GLOB FLEXML_TIMELINE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/flexml_timeline/*.h"
  "${PROFILE_DIR}/plugin/flexml_timeline/*.cpp"
#  "${PROFILE_DIR}/writer/flexml_timeline/*.h"
#  "${PROFILE_DIR}/writer/flexml_timeline/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)

if (XDP_MINIMAL_BUILD STREQUAL "yes")
  add_library(xdp_flexml_timeline_plugin MODULE ${FLEXML_TIMELINE_PLUGIN_FILES})
  add_dependencies(xdp_flexml_timeline_plugin xdp_core xrt_coreutil)

  include_directories(xdp_flexml_timeline_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win/op)
  target_link_libraries(xdp_flexml_timeline_plugin PRIVATE xdp_core xrt_coreutil xaiengine)

  target_compile_definitions(xdp_flexml_timeline_plugin PRIVATE XDP_MINIMAL_BUILD=1)

  target_include_directories(xdp_flexml_timeline_plugin PRIVATE ${AIERT_DIR}/include)

  set_target_properties(xdp_flexml_timeline_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_flexml_timeline_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
  )

endif()
