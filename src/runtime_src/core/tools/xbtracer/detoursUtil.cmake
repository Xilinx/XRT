# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

set(DETOURS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Detours")
file(GLOB DETOURS_SRC_FILES
  "${DETOURS_ROOT}/src/*.cpp"
)

# This file is included and not compiled on its own, it needs to be compiled
# with creatwth.cpp.
set_property (
  SOURCE ${DETOURS_ROOT}/src/uimports.cpp
  APPEND PROPERTY HEADER_FILE_ONLY TRUE
)

add_library(ms_detours STATIC ${DETOURS_SRC_FILES})
target_compile_options(ms_detours PRIVATE /nologo /W4 /WX /we4777 /we4800 /Zi /MT /Gy /Gm- /Zl /Od)
target_include_directories(ms_detours PUBLIC "${DETOURS_ROOT}/src")
set(DETOURS_INCLUDE "${DETOURS_ROOT}/src")
