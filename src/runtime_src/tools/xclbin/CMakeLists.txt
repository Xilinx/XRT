include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/include
  )

file(GLOB XCLBINCAT_FILES
  "xclbincat*.cxx"
  "xclbincat*.h"
  "xclbindata.*"
  "xclbinutil.*"
  )

set(XCLBINCAT_SRC ${XCLBINCAT_FILES})
add_executable(xclbincat ${XCLBINCAT_SRC})

file(GLOB XCLBINSPLIT_FILES
  "xclbinsplit*.cxx"
  "xclbinsplit*.h"
  "xclbindata.*"
  "xclbinutil.*"
  )

set(XCLBINSPLIT_SRC ${XCLBINSPLIT_FILES})
add_executable(xclbinsplit ${XCLBINSPLIT_SRC})

install (TARGETS xclbincat xclbinsplit RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
