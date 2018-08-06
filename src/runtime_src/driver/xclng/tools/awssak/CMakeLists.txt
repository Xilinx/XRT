include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_aws
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ../../../../../../../sdk/userspace/include/ # path to fpga_mgmt.h
  )

file(GLOB AWSSAK_FILES
  "*.h"
  "*.cpp"
  )

set(AWSSAK_SRC ${AWSSAK_FILES})

add_executable(awssak ${AWSSAK_SRC})

if(${INTERNAL_TESTING})
  add_definitions(-DINTERNAL_TESTING)
  target_link_libraries(awssak
    xrt-awsstatic
    pthread
    rt
    )
else(${INTERNAL_TESTING})
  set(AWS_FPGA_MGMT_LIB_DIR ../../../../../../../sdk/userspace/lib)
  
  target_link_libraries(awssak
    xrt-awsstatic
    pthread
    rt
    ${AWS_FPGA_MGMT_LIB_DIR}/libfpga_mgmt.a
    )
endif(${INTERNAL_TESTING})

install (TARGETS awssak RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

