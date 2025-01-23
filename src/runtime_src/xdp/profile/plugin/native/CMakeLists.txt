# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the Native XRT API plugin, which creates a timeline
# trace of the calls to Native XRT APIs.
# ====================================================================

file(GLOB NATIVE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/native/*.h"
  "${PROFILE_DIR}/plugin/native/*.cpp"
  "${PROFILE_DIR}/writer/native/*.h"
  "${PROFILE_DIR}/writer/native/*.cpp"
)

add_library(xdp_native_plugin MODULE ${NATIVE_PLUGIN_FILES})
add_dependencies(xdp_native_plugin xdp_core xrt_coreutil)
target_link_libraries(xdp_native_plugin PRIVATE xdp_core xrt_coreutil)

set_target_properties(xdp_native_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_native_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
