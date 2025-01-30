# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the Power monitoring plugin, which is only available on
# Linux hardware.  It has a dependency on the HW shim.
# ====================================================================

file(GLOB POWER_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/power/*.h"
  "${PROFILE_DIR}/plugin/power/*.cpp"
  "${PROFILE_DIR}/writer/power/*.h"
  "${PROFILE_DIR}/writer/power/*.cpp"
)

add_library(xdp_power_plugin MODULE ${POWER_PLUGIN_FILES})
add_dependencies(xdp_power_plugin xdp_core xrt_core)
target_link_libraries(xdp_power_plugin PRIVATE xdp_core xrt_core)

set_target_properties(xdp_power_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_power_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
