include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_aws
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ../../../../../../../aws-fpga-staging/sdk/userspace/include/ # path to fpga_mgmt.h
  )

file(GLOB AWSSAK_FILES
  "*.h"
  "*.cpp"
  )

set(AWSSAK_SRC ${AWSSAK_FILES})

add_executable(awssak ${AWSSAK_SRC})

set(AWS_FPGA_MGMT_LIB_DIR ../../../../../aws-fpga-staging/sdk/userspace/lib/)
find_library(AWS_FPGA_MGMT_LIB_DIR fpga_mgmt)

target_link_libraries(awssak
  xrt-awsstatic
  pthread
  rt
  fpga_mgmt #"${AWS_FPGA_MGMT_LIB_DIR}"
  )

install (TARGETS awssak RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

