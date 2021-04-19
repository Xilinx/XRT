PROJECT(XRT)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5.0)

SET(PROJECT_DESCRIPTION "https://github.com/Xilinx/XRT")


set (XRT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

set(XRT_VERSION_STRING "cross_compile")
set (XRT_INSTALL_DIR           "${CMAKE_BINARY_DIR}")
set (XRT_INSTALL_INCLUDE_DIR   "${XRT_INSTALL_DIR}/include")
set(XRT_NATIVE_BUILD "no")


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${XRT_SRC_DIR}/CMake/")
set(XRT_DKMS_DRIVER_SRC_BASE_DIR "${XRT_SRC_DIR}/runtime_src/core")
set(XRT_DKMS_INSTALL_DIR "${CMAKE_BINARY_DIR}/../driver_code")
set(CMAKE_SOURCE_DIR "${XRT_SRC_DIR}")

include(${XRT_SRC_DIR}/CMake/version.cmake)
include(${XRT_SRC_DIR}/CMake/dkms.cmake)

#install version.h
install(FILES ${XRT_INSTALL_INCLUDE_DIR}/version.h DESTINATION ${XRT_DKMS_INSTALL_DIR}/driver/include)

#install LICENSE for petalinux build
install(FILES ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/pcie/driver/linux/xocl/LICENSE DESTINATION ${XRT_DKMS_INSTALL_DIR}/driver/xocl)
