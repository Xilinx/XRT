# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the NoC plugin, which is currently unused and not
# completely implemented, but included for future expansion.
# ====================================================================

file(GLOB NOC_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/noc/*.h"
  "${PROFILE_DIR}/plugin/noc/*.cpp"
  "${PROFILE_DIR}/writer/noc/*.h"
  "${PROFILE_DIR}/writer/noc/*.cpp"
)

add_library(xdp_noc_plugin MODULE ${NOC_PLUGIN_FILES})
add_dependencies(xdp_noc_plugin xdp_core xrt_core)
target_link_libraries(xdp_noc_plugin PRIVATE xdp_core xrt_core)

set_target_properties(xdp_noc_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_noc_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
