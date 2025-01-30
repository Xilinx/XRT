# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# ====================================================================
# This builds the Low Overhead OpenCL API plugin.  This is a lower
# overhead trace than the full OpenCL trace.
# ====================================================================

file(GLOB LOP_PLUGIN_FILES
  "${PROFILE_DIR}/plugin/lop/*.h"
  "${PROFILE_DIR}/plugin/lop/*.cpp"
  "${PROFILE_DIR}/writer/lop/*.h"
  "${PROFILE_DIR}/writer/lop/*.cpp"
)

add_library(xdp_lop_plugin MODULE ${LOP_PLUGIN_FILES})
add_dependencies(xdp_lop_plugin xdp_core xrt_coreutil)
target_link_libraries(xdp_lop_plugin PRIVATE xdp_core xrt_coreutil)

set_target_properties(xdp_lop_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_lop_plugin LIBRARY DESTINATION ${XDP_PLUGIN_INSTALL_DIR} COMPONENT ${XRT_COMPONENT})
