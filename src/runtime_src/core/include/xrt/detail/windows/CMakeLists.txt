# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

# These headers were moved from runtime_src/core/include/windows.
# Must be installed to legacy include dir for legacy xrt package as
# well as xrt/detail include dir for base package
set(XRT_WINDOWS_HEADER_SRC
  types.h
  uuid.h)

# Legacy deprecated install
install (FILES ${XRT_WINDOWS_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/windows
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

# base component install
install (FILES ${XRT_WINDOWS_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail/windows
  COMPONENT ${XRT_BASE_DEV_COMPONENT})
