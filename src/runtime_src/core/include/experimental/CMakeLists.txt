# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
set(XRT_EXPERIMENTAL_HEADER_SRC
  xrt-next.h
  xrt_aie.h
  xrt_graph.h
  xrt_bo.h
  xrt_device.h
  xrt_elf.h
  xrt_error.h
  xrt_exception.h
  xrt_ext.h
  xrt_fence.h
  xrt_hw_context.h
  xrt_ini.h
  xrt_ip.h
  xrt_kernel.h
  xrt_mailbox.h
  xrt_message.h
  xrt_module.h
  xrt_profile.h
  xrt_queue.h
  xrt_system.h
  xrt_uuid.h
  xrt_version.h
  xrt_xclbin.h
  xclbin_util.h
  xclbin-util.h)

# Legacy deprecated install
install (FILES ${XRT_EXPERIMENTAL_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental
  COMPONENT ${XRT_BASE_DEV_COMPONENT})
