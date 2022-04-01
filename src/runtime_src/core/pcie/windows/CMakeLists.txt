# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
if (MCDM_IN_XRT)
  add_subdirectory(mcdm)
elseif (NOT MCDM)
  add_subdirectory(alveo)
endif()
