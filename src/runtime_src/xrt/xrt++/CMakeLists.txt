# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Preserve legacy behavior in legacy XRT package. The component
# setup will have defined XRT_BASE_DEV_COMPONENT to XRT
if (XRT_XRT)
  set(XRT_XRT_CPP_HEADER_SRC
    xrt++.hpp
    xrtexec.hpp)

  install (FILES ${XRT_XRT_CPP_HEADER_SRC}
    DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental COMPONENT ${XRT_BASE_DEV_COMPONENT})
endif (XRT_XRT)
