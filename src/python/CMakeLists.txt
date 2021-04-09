# SPDX-License-Identifier: Apache-2.0

add_subdirectory(pybind11)

install(FILES xrt_binding.py    DESTINATION ${PY_INSTALL_DIR})
install(FILES ert_binding.py    DESTINATION ${PY_INSTALL_DIR})
install(FILES xclbin_binding.py DESTINATION ${PY_INSTALL_DIR})
