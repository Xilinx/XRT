# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the HW Emu device offload plugin.  It is responsible for
# collecting device information when the host application uses Native
# XRT APIs or lower to control execution during hardware emulation.
# It should only be built on Linux.
# ====================================================================

file(GLOB HW_EMU_DEVICE_OFFLOAD_PLUGIN_FILES
  "${PROFILE_DIR}/device/hal_device/*.h"
  "${PROFILE_DIR}/device/hal_device/*.cpp"
  "${PROFILE_DIR}/plugin/device_offload/*.h"
  "${PROFILE_DIR}/plugin/device_offload/*.cpp"
  "${PROFILE_DIR}/plugin/device_offload/hw_emu/*.h"
  "${PROFILE_DIR}/plugin/device_offload/hw_emu/*.cpp"
  "${PROFILE_DIR}/writer/device_trace/*.h"
  "${PROFILE_DIR}/writer/device_trace/*.cpp"
)

add_library(xdp_hw_emu_device_offload_plugin MODULE ${HW_EMU_DEVICE_OFFLOAD_PLUGIN_FILES})
add_dependencies(xdp_hw_emu_device_offload_plugin xdp_core xrt_coreutil xrt_hwemu)
target_link_libraries(xdp_hw_emu_device_offload_plugin PRIVATE xdp_core xrt_coreutil xrt_hwemu)
target_compile_definitions(xdp_hw_emu_device_offload_plugin PRIVATE XDP_HWEMU_USING_HAL_BUILD=1)

set_target_properties(xdp_hw_emu_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_hw_emu_device_offload_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
