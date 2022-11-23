# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Profile plugin.  It is currently only built
# on Edge platforms that support AIE.
# ====================================================================

if (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")

file(GLOB AIE_PROFILE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/aie_profile/*.h"
  "${PROFILE_DIR}/plugin/aie_profile/*.cpp"
  "${PROFILE_DIR}/writer/aie_profile/*.h"
  "${PROFILE_DIR}/writer/aie_profile/*.cpp"
)

add_library(xdp_aie_profile_plugin MODULE ${AIE_PROFILE_PLUGIN_FILES})
add_dependencies(xdp_aie_profile_plugin xdp_core)
target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core metal xaiengine)

set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_aie_profile_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
)

endif()
