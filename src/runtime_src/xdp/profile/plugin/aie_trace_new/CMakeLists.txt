# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the AIE Trace plugin.  It is currently built on x86 and
# Edge-Versal systems, but not Edge-aarch64.  It also has a dependency
# on the hardware shim
# ====================================================================

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace_new/x86")
elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  set(IMPL_DIR "${PROFILE_DIR}/plugin/aie_trace_new/edge")
endif()

file(GLOB AIE_TRACE_PLUGIN_FILES
  "${PROFILE_DIR}/device/aie_trace/*.h"
  "${PROFILE_DIR}/device/aie_trace/*.cpp"
  "${PROFILE_DIR}/device/hal_device/*.h"
  "${PROFILE_DIR}/device/hal_device/*.cpp"
  "${PROFILE_DIR}/plugin/aie_trace_new/*.h"
  "${PROFILE_DIR}/plugin/aie_trace_new/*.cpp"
  "${PROFILE_DIR}/writer/aie_trace/*.h"
  "${PROFILE_DIR}/writer/aie_trace/*.cpp"
  "${IMPL_DIR}/*.h"
  "${IMPL_DIR}/*.cpp"
)

if (${XRT_NATIVE_BUILD} STREQUAL "yes")

  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES})

  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_core)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_core)
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE XRT_X86_BUILD=1)

  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
  )

elseif (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")

  add_library(xdp_aie_trace_plugin MODULE ${AIE_TRACE_PLUGIN_FILES})

  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_core)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_core metal xaiengine)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR}
  )

# Else, on edge-aarch64 don't build at all

endif()


