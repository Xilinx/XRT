# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

set(PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Build the XDP core library here.  We build the same for Windows and Linux.
file(GLOB XDP_CORE_FILES
  "${PROFILE_DIR}/plugin/vp_base/*.h"
  "${PROFILE_DIR}/plugin/vp_base/*.cpp"
  "${PROFILE_DIR}/database/*.h"
  "${PROFILE_DIR}/database/*.cpp"
  "${PROFILE_DIR}/database/dynamic_info/*.h"
  "${PROFILE_DIR}/database/dynamic_info/*.cpp"
  "${PROFILE_DIR}/database/events/*.h"
  "${PROFILE_DIR}/database/events/*.cpp"
  "${PROFILE_DIR}/database/events/creator/*.h"
  "${PROFILE_DIR}/database/events/creator/*.cpp"
  "${PROFILE_DIR}/database/static_info/*.h"
  "${PROFILE_DIR}/database/static_info/*.cpp"
  "${PROFILE_DIR}/database/static_info/filetypes/*.cpp"
  "${PROFILE_DIR}/database/static_info/filetypes/*.h"
  "${PROFILE_DIR}/device/*.h"
  "${PROFILE_DIR}/device/*.cpp"
  "${PROFILE_DIR}/device/ioctl_monitors/*.h"
  "${PROFILE_DIR}/device/ioctl_monitors/*.cpp"
  "${PROFILE_DIR}/device/mmapped_monitors/*.h"
  "${PROFILE_DIR}/device/mmapped_monitors/*.cpp"
  "${PROFILE_DIR}/writer/vp_base/*.h"
  "${PROFILE_DIR}/writer/vp_base/*.cpp"
  )

add_library(xdp_core SHARED ${XDP_CORE_FILES})
add_dependencies(xdp_core xrt_coreutil)

if (XDP_CLIENT_BUILD_CMAKE STREQUAL "yes")
  target_compile_definitions(xdp_core PRIVATE XDP_CLIENT_BUILD=1)
endif()

target_link_libraries(xdp_core PRIVATE xrt_coreutil)

set_target_properties(xdp_core PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

# xdp_core is not an end-user link library, so skip the name link
# and do not add to xrt-targets.
install(TARGETS xdp_core
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_COMPONENT} NAMELINK_SKIP)

# Build the individual plugins
add_subdirectory(plugin)
