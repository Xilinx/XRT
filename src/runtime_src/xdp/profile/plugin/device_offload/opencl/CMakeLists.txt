# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the OpenCL device offload plugin.  It is responsible for
# collecting device information when the host application uses OpenCL
# APIs to control execution.
# ====================================================================

file(GLOB OPENCL_DEVICE_OFFLOAD_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/device_offload/*.h"
  "${PROFILE_DIR}/plugin/device_offload/*.cpp"
  "${PROFILE_DIR}/plugin/device_offload/opencl/*.h"
  "${PROFILE_DIR}/plugin/device_offload/opencl/*.cpp"
  "${PROFILE_DIR}/writer/device_trace/*.h"
  "${PROFILE_DIR}/writer/device_trace/*.cpp"
)

add_library(xdp_device_offload_plugin MODULE ${OPENCL_DEVICE_OFFLOAD_PLUGIN_FILES})
add_dependencies(xdp_device_offload_plugin xdp_core xilinxopencl xrt_coreutil)
target_link_libraries(xdp_device_offload_plugin PRIVATE xdp_core xilinxopencl xrt_coreutil)

set_target_properties(xdp_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_device_offload_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT}
)
