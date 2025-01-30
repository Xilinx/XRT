# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

# These headers must be installed to legacy include dir for legacy xrt
# package as well as under xrt/ for base package and also xrt legacy
# The includes simply refer into include/xrt
set(XRT_CORE_HEADER_SRC
  ert.h
  xrt.h
  xclbin.h
  xclerr.h
  xrt_error_code.h
  xrt_mem.h

  # unknown where used
  xgq_cmd_common.h
  xgq_cmd_ert.h
  xgq_impl.h
  xgq_resp_parser.h)


# Legacy deprecated install
install (FILES ${XRT_CORE_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

# Legacy experimental
add_subdirectory(experimental)

# Exported includes
add_subdirectory(xrt)

