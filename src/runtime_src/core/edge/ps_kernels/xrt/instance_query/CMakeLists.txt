# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

add_library(instance_query OBJECT
  instance_query.cpp
  )

set_target_properties(instance_query PROPERTIES 
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

install (TARGETS instance_query 
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
