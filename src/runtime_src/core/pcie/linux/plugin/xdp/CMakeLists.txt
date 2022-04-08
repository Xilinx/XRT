# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

file(GLOB XRT_PCIE_LINUX_PLUGIN_XDP_FILES *.cpp)

add_library(core_pcielinux_plugin_xdp_objects OBJECT
  ${XRT_PCIE_LINUX_PLUGIN_XDP_FILES}
  )

target_include_directories(core_pcielinux_plugin_xdp_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/pcie/linux
  )
