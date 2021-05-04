# SPDX-License-Identifier: Apache-2.0

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBFLASH_QSPI_FILES
  "*.h"
  "*.cpp"
  )

set(XBFLASH_QSPI_SRC ${XBFLASH_QSPI_FILES})
set(XBFLASH_NAME "xbflash.qspi")
set(XBFLASH_NAME_NEW "xbflash")
set(XBFLASH_INSTALL_DEST "/usr/local/bin")
set(XBFLASH_COMPONENT "xbflash")

add_executable(${XBFLASH_NAME} ${XBFLASH_QSPI_SRC})
add_executable(${XBFLASH_NAME_NEW} ${XBFLASH_QSPI_SRC})
if (XRT_STATIC_BUILD)
    target_link_options(${XBFLASH_NAME_NEW} PRIVATE "-static")
endif()

set_target_properties(${XBFLASH_NAME_NEW} PROPERTIES INSTALL_RPATH "")
install (
TARGETS ${XBFLASH_NAME}
RUNTIME
DESTINATION ${XRT_INSTALL_DIR}/bin)

install (
TARGETS ${XBFLASH_NAME_NEW}
RUNTIME
DESTINATION ${XBFLASH_INSTALL_DEST}
COMPONENT ${XBFLASH_COMPONENT})
