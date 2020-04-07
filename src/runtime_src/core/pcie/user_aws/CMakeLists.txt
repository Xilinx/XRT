# Vendor - AWS
# Skip for arm
if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

if(${INTERNAL_TESTING})
  add_definitions(-DINTERNAL_TESTING)
else()
  include_directories(${AWS_FPGA_REPO_DIR}/sdk/userspace/include/)
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  )

file(GLOB XRT_USER_AWS_FILES
  "*.h"
  "*.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

add_library(user_aws OBJECT ${XRT_USER_AWS_FILES})

add_library(xrt_aws SHARED ""
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:user_aws>
  )

add_library(xrt_aws_static STATIC ""
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:user_aws>
  )

add_compile_options("-fvisibility=hidden")

set_target_properties(xrt_aws
  PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (${INTERNAL_TESTING})
  target_link_libraries(xrt_aws
  xrt_coreutil
  pthread
  rt
  boost_filesystem
  boost_system
  uuid
  )
else ()
  set(AWS_FPGA_MGMT_LIB_DIR ${AWS_FPGA_REPO_DIR}/sdk/userspace/lib)
  target_link_libraries(xrt_aws
  xrt_coreutil
  pthread
  rt
  boost_filesystem
  boost_system
  uuid
  ${AWS_FPGA_MGMT_LIB_DIR}/libfpga_mgmt.a
)
endif ()

install(TARGETS xrt_aws
        LIBRARY
        DESTINATION ${XRT_INSTALL_LIB_DIR}
        COMPONENT aws)

endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
