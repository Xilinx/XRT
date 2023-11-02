# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Xilinx, Inc. All rights reserved.
#

find_package(Boost
  REQUIRED COMPONENTS system program_options)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

# Collect files outside of this directory
file(GLOB XBFLASH_BASE_FILES
  "../common/XBUtilitiesCore.cpp"
  "../common/SubCmd.cpp"
  "../common/OptionOptions.cpp"
  "../common/XBHelpMenusCore.cpp"
  "../common/JSONConfigurable.cpp"
  "../../pcie/tools/xbflash.qspi/firmware_image.cpp"
  "../../pcie/tools/xbflash.qspi/pcidev.cpp"
  "../../pcie/tools/xbflash.qspi/xqspips.cpp"
  "../../pcie/tools/xbflash.qspi/xspi.cpp"
)

file(GLOB XBFLASH_QSPI_FILES
  "*.cpp"
  )

set(XBFLASH_QSPI_SRC ${XBFLASH_BASE_FILES} ${XBFLASH_QSPI_FILES})
set(XBFLASH_NAME "xbflash2")
set(XBFLASH_INSTALL_DEST "/usr/local/bin")
set(XBFLASH_COMPONENT "xbflash2")

add_executable(${XBFLASH_NAME} ${XBFLASH_QSPI_SRC})

set(Boost_USE_STATIC_LIBS ON)
target_link_libraries(${XBFLASH_NAME}
      PRIVATE
      boost_system
      boost_program_options
      )

set_target_properties(${XBFLASH_NAME} PROPERTIES INSTALL_RPATH "")

install (
TARGETS ${XBFLASH_NAME}
RUNTIME
DESTINATION ${XBFLASH_INSTALL_DEST}
COMPONENT ${XBFLASH_COMPONENT})
