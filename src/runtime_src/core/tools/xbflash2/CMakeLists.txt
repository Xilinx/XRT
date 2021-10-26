# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBFLASH_QSPI_FILES
  "*.h"
  "*.cpp"
  )

set(XBFLASH_QSPI_SRC ${XBFLASH_QSPI_FILES})
set(XBFLASH_NAME "xbflash2")
set(XBFLASH_INSTALL_DEST "/usr/local/bin")
set(XBFLASH_COMPONENT "xbflash2")

add_executable(${XBFLASH_NAME} ${XBFLASH_QSPI_SRC})

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
  REQUIRED COMPONENTS system program_options)
target_link_libraries(${XBFLASH_NAME}
      PRIVATE
      ${Boost_LIBRARIES}
      )

set_target_properties(${XBFLASH_NAME} PROPERTIES INSTALL_RPATH "")

install (
TARGETS ${XBFLASH_NAME}
RUNTIME
DESTINATION ${XBFLASH_INSTALL_DEST}
COMPONENT ${XBFLASH_COMPONENT})
