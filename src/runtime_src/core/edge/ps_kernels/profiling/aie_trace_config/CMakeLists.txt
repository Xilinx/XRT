# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

include_directories(
  ${XRT_SOURCE_DIR}/runtime_src/core/include
  ${XRT_SOURCE_DIR}/runtime_src/
  ${XRT_SOURCE_DIR}/runtime_src/core/edge/include
  )

add_library(aie_trace_config SHARED "aie_trace_config.cpp")

set_target_properties(aie_trace_config PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_compile_definitions(aie_trace_config PRIVATE
  XAIE_DEBUG
  __PS_ENABLE_AIE__
  XCLHAL_MAJOR_VER=1
  XCLHAL_MINOR_VER=0
  FAL_LINUX="on"
)

target_link_libraries(aie_trace_config
    PRIVATE
    xrt_coreutil
    xrt_core
    xilinxopencl
    pthread
    rt
    dl
    crypt
    stdc++
    xaiengine
)

install (TARGETS aie_trace_config
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
