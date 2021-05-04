# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_BIN_DIR

enable_testing()
add_test(NAME xbmgmt
  COMMAND ${CMAKE_BINARY_DIR}/runtime_src/core/pcie/tools/xbmgmt/xbmgmt scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbutil
  COMMAND ${CMAKE_BINARY_DIR}/runtime_src/core/pcie/tools/xbutil/xbutil scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbutil2
  COMMAND ${CMAKE_BINARY_DIR}/runtime_src/core/tools/xbutil2/xbutil2 examine
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbmgmt2
  COMMAND ${CMAKE_BINARY_DIR}/runtime_src/core/tools/xbmgmt2/xbmgmt2 examine -r host
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME python_binding
  COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/../tests/python/200_binding/200_main.py"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(python_binding PROPERTIES ENVIRONMENT
  "PYTHONPATH=.${CMAKE_INSTALL_PREFIX}/${XRT_INSTALL_DIR}/python;XILINX_XRT=.${CMAKE_INSTALL_PREFIX}/${XRT_INSTALL_DIR}")

set_tests_properties(xbutil xbmgmt PROPERTIES ENVIRONMENT INTERNAL_BUILD=1)
