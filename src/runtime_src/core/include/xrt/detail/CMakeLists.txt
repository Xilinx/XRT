# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
set(XRT_XRT_DETAIL_HEADER_SRC
  abi.h
  any.h
  bitmask.h
  config.h
  param_traits.h
  pimpl.h)

# xrt component install
install (FILES ${XRT_XRT_DETAIL_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_DEV_COMPONENT})

# base component install
install (FILES ${XRT_XRT_DETAIL_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

set(XRT_XRT_DETAIL_LEGACY_SRC
  ert.h
  xclbin.h
  xrt_error_code.h
  xrt_mem.h)
  
# xrt component deprecated install
install (FILES ${XRT_XRT_DETAIL_LEGACY_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
  COMPONENT ${XRT_DEV_COMPONENT})

# xrt component install
install (FILES ${XRT_XRT_DETAIL_LEGACY_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_DEV_COMPONENT})

# base component install
install (FILES ${XRT_XRT_DETAIL_LEGACY_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

if (WIN32)
  add_subdirectory(windows)
endif()

