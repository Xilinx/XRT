# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
link_directories($ENV{XILINX_XRT}/lib)

add_library(xrt_capture SHARED
  capture.cpp
  logger.cpp
  xrt_device_inst.cpp
  xrt_kernel_inst.cpp
  xrt_bo_inst.cpp
  xrt_hw_context_inst.cpp
  xrt_xclbin_inst.cpp
  xrt_module_inst.cpp
  xrt_elf_inst.cpp
)

if(WIN32)
  target_link_libraries(xrt_capture PRIVATE xrt_coreutil)
else()
  target_link_libraries(xrt_capture PRIVATE xrt_coreutil uuid dl pthread)
endif()

# xrt_capture is a plug-in runtime loaded library
# it does not have a library or namelink component
install(TARGETS xrt_capture
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT}
)
