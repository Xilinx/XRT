# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

# Legacy header wrappers are no longer released to base package.  Only
# xrt/* are released and applications must include from include/xrt
#
# The legacy headers are simple wrappers that
# refer to include/xrt.  These wrapper headers have included a pragma
# warning for quite a while that have asked applications to include from
# corresponding header files under include/xrt.

# Preserve legacy behavior in legacy XRT package. The component
# setup will have defined XRT_BASE_DEV_COMPONENT to XRT
if (XRT_XRT)

  set(XRT_CORE_HEADER_SRC
    ert.h
    xrt.h
    xclbin.h
    xclerr.h
    xrt_error_code.h
    xrt_mem.h
  )

  # Legacy deprecated install
  install (FILES ${XRT_CORE_HEADER_SRC}
    DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
    COMPONENT ${XRT_BASE_DEV_COMPONENT})

  # Legacy experimental
  add_subdirectory(experimental)

endif (XRT_XRT)

# Exported includes
add_subdirectory(xrt)

