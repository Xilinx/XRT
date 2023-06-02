# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#
add_subdirectory(instance_query)

if (EXISTS "/opt/xilinx/xrt")
  message(WARNING "XILINX_XRT is set! Generating ps kernel xclbins")
  add_subdirectory(tests)
else()
  message(WARNING "XILINX_XRT not set, skip generating ps kernel xclbins")
endif()
