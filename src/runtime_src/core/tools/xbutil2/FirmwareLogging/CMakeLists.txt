# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Collect EventTrace source files with full paths
file(GLOB FIRMWARE_LOG_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add source files to parent's collection
list(APPEND XBUTIL_V2_SUBCMD_FILES ${FIRMWARE_LOG_FILES})
set(XBUTIL_V2_SUBCMD_FILES ${XBUTIL_V2_SUBCMD_FILES} PARENT_SCOPE)
