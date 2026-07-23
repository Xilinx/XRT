# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 Xilinx, Inc. All rights Reserved.
# Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights Reserved.
if(NOT WIN32)
  add_compile_options(-Wextra -fvisibility=default)
else()
  add_compile_options(/wd4244 /wd4267 /wd4477 /wd4245)
endif()

# This builds a static library libtransformcdo, which 
# includes all the *.c files in lib/
add_library( transformcdo STATIC
    pdi-parsing.c
    cdo-parsing.c
    pdi-transform.c
    ../lib/cdo_cmd.c
    ../lib/load_pdi.c
    ../lib/pdi_transform.c
)

target_include_directories(transformcdo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinclude
    ${CMAKE_CURRENT_SOURCE_DIR}
)

