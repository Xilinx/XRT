include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBFLASH_QSPI_FILES
  "*.h"
  "*.cpp"
  )

set(XBFLASH_QSPI_SRC ${XBFLASH_QSPI_FILES})
add_executable(xbflash.qspi ${XBFLASH_QSPI_SRC})

install (TARGETS xbflash.qspi RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
