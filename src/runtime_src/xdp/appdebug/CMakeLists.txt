# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# =======================================================
# Application Debug Plugin Build
# 
# This plugin is only available on Linux and contains
# the functions that are called from the xprint and
# xstatus gdb extension commands
# =======================================================

if (NOT WIN32)

set(APPDEBUG_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB APPDEBUG_FILES
  "${APPDEBUG_DIR}/*.h"
  "${APPDEBUG_DIR}/*.cpp"
  )

# Since the functions in the appdebug plugin are only accessed via gdb,
# we need to compile the plugin with -g so the functions aren't optimized
# away and are visible to gdb.
set_source_files_properties (${APPDEBUG_FILES} PROPERTIES COMPILE_FLAGS -g)

add_library(xdp_appdebug_plugin MODULE ${APPDEBUG_FILES})

# The application debug plugin is specific to OpenCL applications, so
# we have a dependency to xilinxopencl
add_dependencies(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_appdebug_plugin PRIVATE xrt_coreutil xilinxopencl)

set_target_properties(xdp_appdebug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_appdebug_plugin
         LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR})

# We also need to copy the python scripts that contain the gdb connection.
# Only install these files for PCIe device for now, which is native build
if (${XRT_NATIVE_BUILD} STREQUAL "yes")

set (APPDEBUG_INSTALL_PREFIX "/opt/xilinx/xrt/share/appdebug")
install (FILES
    ${APPDEBUG_DIR}/appdebug.py
    ${APPDEBUG_DIR}/appdebugint.py
    DESTINATION ${APPDEBUG_INSTALL_PREFIX})

endif()

endif()