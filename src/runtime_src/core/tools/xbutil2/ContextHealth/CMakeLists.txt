# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

file(GLOB CONTEXT_HEALTH_FILES
  "OO_ContextHealth.cpp"
  "OO_ContextHealth.h"
)

# Append files to the parent scope
list(APPEND XBUTIL_V2_SUBCMD_FILES ${CONTEXT_HEALTH_FILES})
set(XBUTIL_V2_SUBCMD_FILES ${XBUTIL_V2_SUBCMD_FILES} PARENT_SCOPE)

