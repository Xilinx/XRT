# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

# These headers were moved from runtime_src/core/include/deprecated.
# Must be installed to legacy include dir for legacy xrt package as
# well as xrt/deprecated include dir for base package
set(XRT_DEPRECATED_HEADER_SRC
  xrt.h
  xclerr.h
  )

# Base component install
install (FILES ${XRT_DEPRECATED_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/deprecated
  COMPONENT ${XRT_BASE_DEV_COMPONENT})
