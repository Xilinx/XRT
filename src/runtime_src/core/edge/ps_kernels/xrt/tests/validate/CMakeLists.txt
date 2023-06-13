# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

add_subdirectory(ps_aie_test)
add_subdirectory(ps_bandwidth_test)
add_subdirectory(ps_validate_test)

install (FILES test_dependencies.json
  DESTINATION ${PS_KERNEL_INSTALL_DIR}
  )
