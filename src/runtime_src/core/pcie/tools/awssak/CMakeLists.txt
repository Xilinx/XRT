if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
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
    xrt_aws_static
    xrt_coreutil_static
    uuid
    boost_filesystem
    boost_system
    pthread
    rt
    )
else()
  include_directories(${AWS_FPGA_REPO_DIR}/sdk/userspace/include/) # path to fpga_mgmt.h
  set(AWS_FPGA_MGMT_LIB_DIR ${AWS_FPGA_REPO_DIR}/sdk/userspace/lib)

  target_link_libraries(awssak
    xrt_aws_static
    xrt_coreutil_static
    uuid
    boost_filesystem
    boost_system
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
