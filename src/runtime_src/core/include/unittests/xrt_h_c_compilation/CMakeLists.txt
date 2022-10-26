# SPDX-License-Identifier: Apache-2.0                                           
# Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.

add_executable(main main.c)
target_include_directories(main
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../
  )
