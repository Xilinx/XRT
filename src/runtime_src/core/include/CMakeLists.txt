# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.

set(XRT_HEADER_SRC
  ert.h
  xclbin.h
  xrt.h
  xrt_mem.h
  xrt_error_code.h
  xgq_cmd_common.h
  xgq_cmd_ert.h
  xgq_impl.h
  xgq_resp_parser.h)

install (FILES ${XRT_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
  COMPONENT ${XRT_DEV_COMPONENT})

install (FILES ${XRT_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

add_subdirectory(xrt)
add_subdirectory(experimental)
add_subdirectory(deprecated)
if(WIN32)
add_subdirectory(windows)
endif()
add_subdirectory(unittests)
