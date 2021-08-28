# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XRT_EDGE_TOOLS_XBUTIL_FILES
  "*.h"
  "*.cpp"
  )

# Bypass binary targets if clangtidy is enabled,
# same targets are defined for DC
if (${XRT_CLANGTIDY_REVIEW})
  return()
endif()

add_executable(xbutil ${XRT_EDGE_TOOLS_XBUTIL_FILES})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xbutil
    PRIVATE
    xrt_core
    xrt_coreutil
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    pthread
    dl
    rt
    uuid
    ${CURSES_LIBRARIES}
    metal
    xaiengine
    )
else()
  target_link_libraries(xbutil
    PRIVATE
    xrt_core
    xrt_coreutil
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    pthread
    dl
    rt
    uuid
    ${CURSES_LIBRARIES}
    )
endif()

install (TARGETS xbutil RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
