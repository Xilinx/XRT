# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the OpenCL counters plugin.  It has a dependency on
# the xilinxopencl library.
# ====================================================================

file(GLOB OPENCL_COUNTERS_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/opencl/counters/*.h"
  "${PROFILE_DIR}/plugin/opencl/counters/*.cpp"
)

add_library(xdp_opencl_counters_plugin MODULE ${OPENCL_COUNTERS_PLUGIN_FILES})
add_dependencies(xdp_opencl_counters_plugin xdp_core xrt_coreutil xilinxopencl)
target_link_libraries(xdp_opencl_counters_plugin PRIVATE xdp_core xrt_coreutil xilinxopencl)

set_target_properties(xdp_opencl_counters_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_opencl_counters_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
