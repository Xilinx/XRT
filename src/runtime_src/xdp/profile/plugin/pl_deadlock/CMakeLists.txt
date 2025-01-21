# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the PL deadlock detection plugin for both hardware and
# hardware emulation.  Hardware is built for both Windows and Linux
# but the emulation plugin is only built on Linux.  The hardware
# plugin has a dependency on the HW shim.
# ====================================================================

file(GLOB PL_DEADLOCK_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/pl_deadlock/*.h"
  "${PROFILE_DIR}/plugin/pl_deadlock/*.cpp"
  "${PROFILE_DIR}/device/hal_device/*.h"
  "${PROFILE_DIR}/device/hal_device/*.cpp"
  "${PROFILE_DIR}/writer/pl_deadlock/*.h"
  "${PROFILE_DIR}/writer/pl_deadlock/*.cpp"
)

add_library(xdp_pl_deadlock_plugin MODULE ${PL_DEADLOCK_PLUGIN_FILES})
add_dependencies(xdp_pl_deadlock_plugin xdp_core xrt_core xrt_coreutil)
target_link_libraries(xdp_pl_deadlock_plugin PRIVATE xdp_core xrt_core xrt_coreutil)

set_target_properties(xdp_pl_deadlock_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_pl_deadlock_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)

if (NOT WIN32)

file(GLOB HW_EMU_PL_DEADLOCK_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/pl_deadlock/hw_emu/*.h"
  "${PROFILE_DIR}/plugin/pl_deadlock/hw_emu/*.cpp"
)

add_library(xdp_hw_emu_pl_deadlock_plugin MODULE ${HW_EMU_PL_DEADLOCK_PLUGIN_FILES})
add_dependencies(xdp_hw_emu_pl_deadlock_plugin xdp_core xrt_coreutil xrt_hwemu)
target_link_libraries(xdp_hw_emu_pl_deadlock_plugin PRIVATE xdp_core xrt_coreutil xrt_hwemu)

set_target_properties(xdp_hw_emu_pl_deadlock_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_hw_emu_pl_deadlock_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)

endif()
