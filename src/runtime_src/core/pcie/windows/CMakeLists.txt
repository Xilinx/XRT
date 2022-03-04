# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
if (MCDM)
  add_subdirectory(mcdm)
else()
  add_subdirectory(alveo)
endif()
