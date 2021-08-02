# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(MPSOC_HEADER_SRC
  xclhal2_mpsoc.h
  sk_types.h)

install (FILES ${MPSOC_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR})

message("-- XRT header files for MPSoC only")
message("-- xclhal2_mpsoc.h")
message("-- sk_types.h")
