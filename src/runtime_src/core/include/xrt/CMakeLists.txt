# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
set(XRT_XRT_HEADER_SRC
  xrt_aie.h
  xrt_bo.h
  xrt_device.h
  xrt_graph.h
  xrt_hw_context.h
  xrt_kernel.h
  xrt_uuid.h)

# base component install
install (FILES ${XRT_XRT_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

add_subdirectory(detail)
add_subdirectory(experimental)
add_subdirectory(deprecated)
