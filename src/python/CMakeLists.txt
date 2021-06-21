# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

add_subdirectory(pybind11)

install(FILES xrt_binding.py    DESTINATION ${PY_INSTALL_DIR})
install(FILES ert_binding.py    DESTINATION ${PY_INSTALL_DIR})
install(FILES xclbin_binding.py DESTINATION ${PY_INSTALL_DIR})
