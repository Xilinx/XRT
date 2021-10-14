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
if (XRT_STATIC_BUILD)
target_link_options(${XBFLASH_NAME} PRIVATE "-static" "-L${Boost_LIBRARY_DIRS}")
target_link_libraries(${XBFLASH_NAME}
      PRIVATE
      boost_system
      boost_filesystem
      boost_program_options
      )
endif()

target_link_libraries(${XBFLASH_NAME}
  PRIVATE
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )


set_target_properties(${XBFLASH_NAME} PROPERTIES INSTALL_RPATH "")

install (
TARGETS ${XBFLASH_NAME}
RUNTIME
DESTINATION ${XBFLASH_INSTALL_DEST}
COMPONENT ${XBFLASH_COMPONENT})
