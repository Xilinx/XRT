# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(XRT_HEADER_SRC
  mailbox_proto.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR} COMPONENT ${XRT_DEV_COMPONENT})

message("-- XRT header files")
foreach (header ${XRT_HEADER_SRC})
  message("-- ${header}")
endforeach()
