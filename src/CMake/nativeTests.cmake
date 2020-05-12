# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_BIN_DIR

enable_testing()
add_test(NAME xbmgmt
  COMMAND ${CMAKE_BINARY_DIR}/${XRT_INSTALL_BIN_DIR}/xbmgmt scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbutil
  COMMAND ${CMAKE_BINARY_DIR}/${XRT_INSTALL_BIN_DIR}/xbutil scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbutil2
  COMMAND ${CMAKE_BINARY_DIR}/${XRT_INSTALL_BIN_DIR}/xbutil --new scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME xbmgmt2
  COMMAND ${CMAKE_BINARY_DIR}/${XRT_INSTALL_BIN_DIR}/xbmgmt --new scan
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(xbutil xbmgmt PROPERTIES ENVIRONMENT INTERNAL_BUILD=1)
