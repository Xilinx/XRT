# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

add_subdirectory(pybind11)

install(FILES xrt_binding.py    DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install(FILES ert_binding.py    DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install(FILES xclbin_binding.py DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT})
