# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the VART plugin.  It adds files to the run summary
# generated from a different layer, so this plugin only has
# a dependency on xdp_core.
# ====================================================================

file(GLOB VART_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/vart/*.h"
  "${PROFILE_DIR}/plugin/vart/*.cpp"
)

add_library(xdp_vart_plugin MODULE ${VART_PLUGIN_FILES})
add_dependencies(xdp_vart_plugin xdp_core)
target_link_libraries(xdp_vart_plugin PRIVATE xdp_core)

set_target_properties(xdp_vart_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_vart_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT})
