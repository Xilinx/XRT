# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
add_subdirectory(include)

set(XRT_KERNEL_HEADERS
  include/xocl_ioctl.h
)

install(FILES ${XRT_KERNEL_HEADERS} DESTINATION ${XRT_INSTALL_DIR}/include)

