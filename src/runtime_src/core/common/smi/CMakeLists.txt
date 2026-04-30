# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#
# OBJECT libraries linked into xrt_coreutil, xrt_core (Alveo/edge), and xrt_driver_xdna / other shims.

add_library(core_common_smi_objects OBJECT smi.cpp)
target_include_directories(core_common_smi_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

add_library(core_smi_alveo_objects OBJECT smi_alveo.cpp)
add_library(core_smi_ryzen_objects OBJECT smi_ryzen.cpp)
