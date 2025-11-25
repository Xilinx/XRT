# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Collect EventTrace source files with full paths
file(GLOB EVENT_TRACE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add source files to parent's collection
list(APPEND XBUTIL_V2_SUBCMD_FILES ${EVENT_TRACE_FILES})
set(XBUTIL_V2_SUBCMD_FILES ${XBUTIL_V2_SUBCMD_FILES} PARENT_SCOPE)
