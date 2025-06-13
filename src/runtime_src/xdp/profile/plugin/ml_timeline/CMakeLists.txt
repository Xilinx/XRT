# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023-2025 Advanced Micro Devices, Inc.  All rights reserved.
#

# ========================================================================================
# This builds the ML Timeline plugin. 
# It is currently built for Client and NPU3 on Windows and Linux,
# and for VE2 Devices on Linux.
# ========================================================================================

if (XDP_NPU3_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/ml_timeline/npu3")
elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/ml_timeline/clientDev")
elseif (XDP_VE2_BUILD_CMAKE STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/ml_timeline/ve2")
endif()

file(GLOB ML_TIMELINE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/ml_timeline/*.h"
  "${PROFILE_DIR}/plugin/ml_timeline/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)

if (XDP_NPU3_BUILD_CMAKE STREQUAL "yes")
  add_library(xdp_ml_timeline_plugin MODULE ${ML_TIMELINE_PLUGIN_FILES})
  add_dependencies(xdp_ml_timeline_plugin xdp_core xrt_coreutil)

  target_link_libraries(xdp_ml_timeline_plugin PRIVATE xdp_core xrt_coreutil)
  target_compile_definitions(xdp_ml_timeline_plugin PRIVATE XDP_NPU3_BUILD=1)
  set_target_properties(xdp_ml_timeline_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_ml_timeline_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

elseif (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
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

  target_link_libraries(xdp_ml_timeline_plugin PRIVATE xdp_core xrt_coreutil)
  target_compile_definitions(xdp_ml_timeline_plugin PRIVATE XDP_VE2_BUILD=1)
  set_target_properties(xdp_ml_timeline_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_ml_timeline_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
  )

else()

# Else, on edge-aarch64 don't build at all

endif()
