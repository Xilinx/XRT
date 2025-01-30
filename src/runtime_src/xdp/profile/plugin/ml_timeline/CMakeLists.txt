# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ========================================================================================
# This builds the ML Timeline plugin. 
# It is currently built for Client on Windows and Linux, and also for VE2 Device on Linux.
# ========================================================================================

if (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/ml_timeline/clientDev")
endif()

if (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/ml_timeline/ve2")
endif()

file(GLOB ML_TIMELINE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/ml_timeline/*.h"
  "${PROFILE_DIR}/plugin/ml_timeline/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)

if (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_ml_timeline_plugin MODULE ${ML_TIMELINE_PLUGIN_FILES})
  add_dependencies(xdp_ml_timeline_plugin xdp_core xrt_coreutil)

  target_link_libraries(xdp_ml_timeline_plugin PRIVATE xdp_core xrt_coreutil)

  target_compile_definitions(xdp_ml_timeline_plugin PRIVATE XDP_CLIENT_BUILD=1)

  set_target_properties(xdp_ml_timeline_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_ml_timeline_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (XDP_VE2_BUILD_CMAKE STREQUAL "yes")

add_library(xdp_ml_timeline_plugin MODULE ${ML_TIMELINE_PLUGIN_FILES})
add_dependencies(xdp_ml_timeline_plugin xdp_core xrt_coreutil)

target_include_directories(xdp_ml_timeline_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(xdp_ml_timeline_plugin PRIVATE xdp_core xrt_coreutil)

target_compile_definitions(xdp_ml_timeline_plugin PRIVATE XDP_VE2_BUILD=1)

set_target_properties(xdp_ml_timeline_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_ml_timeline_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)

endif()
