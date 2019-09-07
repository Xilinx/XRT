if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

if(DEFINED ENV{AWS_FPGA_REPO_DIR} )
    set(AWS_FPGA_REPO_DIR $ENV{AWS_FPGA_REPO_DIR})
    set(INTERNAL_TESTING_FOR_AWS 0)
else()
    set(INTERNAL_TESTING_FOR_AWS 1)
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

file(GLOB AWS_PLUGIN_FILES
  "aws_dev.h"
  "aws_dev.cpp"
  )

add_library(aws_plugin OBJECT ${AWS_PLUGIN_FILES})

add_library(aws_mpd_plugin SHARED ""
  $<TARGET_OBJECTS:aws_plugin>
)

add_compile_options("-fPIC")

if(${INTERNAL_TESTING_FOR_AWS})
  add_definitions(-DINTERNAL_TESTING_FOR_AWS)
  target_link_libraries(aws_mpd_plugin
	xrt_core_static
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

  target_link_libraries(aws_mpd_plugin
	xrt_core_static
	xrt_coreutil_static
    uuid
    boost_filesystem
    boost_system
    pthread
    rt
    ${AWS_FPGA_MGMT_LIB_DIR}/libfpga_mgmt.a
    )
endif()

install(TARGETS aws_mpd_plugin
  LIBRARY
  DESTINATION ${XRT_INSTALL_DIR}/lib
  COMPONENT aws
  )

endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
