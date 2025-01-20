# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the HAL trace plugin which creates a timeline trace of
# calls to hardware shim functions.
# ====================================================================

file(GLOB HAL_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/hal/*.h"
  "${PROFILE_DIR}/plugin/hal/*.cpp"
  "${PROFILE_DIR}/writer/hal/*.h"
  "${PROFILE_DIR}/writer/hal/*.cpp"
)

add_library(xdp_hal_plugin MODULE ${HAL_PLUGIN_FILES})
add_dependencies(xdp_hal_plugin xdp_core xrt_coreutil)
target_link_libraries(xdp_hal_plugin PRIVATE xdp_core xrt_coreutil)

set_target_properties(xdp_hal_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_hal_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
