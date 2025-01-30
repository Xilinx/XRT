# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the HAL API interface plugin.  This provides users
# functions they can call to directly read the registers on profile
# monitors from their host code.  This only works on hardware and
# has a direct depedency on the HW shim.
# ====================================================================

file(GLOB HAL_API_PLUGIN_FILES
  "${PROFILE_DIR}/device/hal_device/*.h"
  "${PROFILE_DIR}/device/hal_device/*.cpp"
  "${PROFILE_DIR}/plugin/hal_api_interface/*.h"
  "${PROFILE_DIR}/plugin/hal_api_interface/*.cpp"
)

add_library(xdp_hal_api_interface_plugin MODULE ${HAL_API_PLUGIN_FILES})
add_dependencies(xdp_hal_api_interface_plugin xdp_core xrt_core xrt_coreutil)
target_link_libraries(xdp_hal_api_interface_plugin PRIVATE xdp_core xrt_core xrt_coreutil)

set_target_properties(xdp_hal_api_interface_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_hal_api_interface_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT})
