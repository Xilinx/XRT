# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(XRT_HEADER_SRC
  ert.h
  xstream.h
  types.h
  xcl_axi_checker_codes.h
  xclbin.h
  xclerr.h
  xclfeatures.h
  xclhal2.h
  xrt.h
  xcl_app_debug.h
  xcl_macros.h
  xcl_app_debug.h
  xclperf.h
  xclhal2_mem.h
  xrt_mem.h
  xrt_error_code.h
  xgq.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR} COMPONENT ${XRT_DEV_COMPONENT})

add_subdirectory(xrt)
add_subdirectory(experimental)
add_subdirectory(deprecated)
if(WIN32)
add_subdirectory(windows)
endif()
