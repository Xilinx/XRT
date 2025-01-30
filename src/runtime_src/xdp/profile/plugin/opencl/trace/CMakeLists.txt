# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the OpenCL trace plugin.  This creates a timeline trace
# of the OpenCL API calls.
# ====================================================================

file(GLOB OPENCL_TRACE_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/opencl/trace/*.h"
  "${PROFILE_DIR}/plugin/opencl/trace/*.cpp"
  "${PROFILE_DIR}/writer/opencl/*.h"
  "${PROFILE_DIR}/writer/opencl/*.cpp"
)

add_library(xdp_opencl_trace_plugin MODULE ${OPENCL_TRACE_PLUGIN_FILES})
add_dependencies(xdp_opencl_trace_plugin xdp_core xrt_coreutil)
target_link_libraries(xdp_opencl_trace_plugin PRIVATE xdp_core xrt_coreutil)

set_target_properties(xdp_opencl_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_opencl_trace_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
