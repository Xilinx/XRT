# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the System Compiler plugin.
# ====================================================================

file(GLOB SYSTEM_COMPILER_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/system_compiler/*.h"
  "${PROFILE_DIR}/plugin/system_compiler/*.cpp"
)

add_library(xdp_system_compiler_plugin MODULE ${SYSTEM_COMPILER_PLUGIN_FILES})
add_dependencies(xdp_system_compiler_plugin xdp_core)
target_link_libraries(xdp_system_compiler_plugin PRIVATE xdp_core)

set_target_properties(xdp_system_compiler_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_system_compiler_plugin
  LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT})
