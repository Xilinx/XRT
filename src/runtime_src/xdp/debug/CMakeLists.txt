# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ==================================================================
# Debug Plugin Build
#
# This plugin is only available on Linux and contains the functions
# that pass information from the debug server to the running process
# in order to support gdb based debug of software emulation kernel
# executions.  This is accessed in OpenCL software emulation so it
# has a dependency on the OpenCL library.
# ==================================================================

if (NOT WIN32)

set(DEBUG_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB DEBUG_FILES
  "${DEBUG_DIR}/*.h"
  "${DEBUG_DIR}/*.cpp"
  )

add_library(xdp_debug_plugin MODULE ${DEBUG_FILES})

add_dependencies(xdp_debug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_debug_plugin PRIVATE xrt_coreutil xilinxopencl)

set_target_properties(xdp_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_debug_plugin
         LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR})

endif()
