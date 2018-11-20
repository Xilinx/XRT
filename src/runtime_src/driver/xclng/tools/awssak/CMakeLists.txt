if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_aws
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
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
    xrt_awsstatic
    pthread
    rt
    )
else()
  include_directories(../../../../../../../sdk/userspace/include/) # path to fpga_mgmt.h
  set(AWS_FPGA_MGMT_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../sdk/userspace/lib)

  target_link_libraries(awssak
    xrt_awsstatic
    pthread
    rt
    ${AWS_FPGA_MGMT_LIB_DIR}/libfpga_mgmt.a
    )
endif()

install(TARGETS awssak
        RUNTIME
        DESTINATION ${XRT_INSTALL_DIR}/bin
        COMPONENT aws)

endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
