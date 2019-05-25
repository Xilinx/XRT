include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/user_gem/
  )

set(XRT_SRC "")

add_library(xrt_core SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:util>
  $<TARGET_OBJECTS:user_gem>
  )

add_library(xrt_core_static STATIC ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:util>
  $<TARGET_OBJECTS:user_gem>
  )

# Core XRT
set_target_properties(xrt_core PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC" "-fvisibility=hidden")
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(user_common)
add_subdirectory(user_gem)

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_core
  xrt_coreutil
  pthread
  rt
  boost_filesystem
  boost_system
  uuid
  )

install(TARGETS xrt_core
        LIBRARY
        DESTINATION ${XRT_INSTALL_DIR}/lib)

# Vendor - AWS
# Skip for arm
if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

add_library(xrt_aws SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:util>
  $<TARGET_OBJECTS:user_aws>
  )

add_library(xrt_awsstatic STATIC ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:util>
  $<TARGET_OBJECTS:user_aws>
  )

set_target_properties(xrt_aws PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC" "-fvisibility=hidden")

add_subdirectory(user_aws)

set_target_properties(xrt_aws PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if(${INTERNAL_TESTING})
  target_link_libraries(xrt_aws
  xrt_coreutil
  pthread
  rt
  boost_filesystem
  boost_system
  uuid
  )
else()
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
endif()

install(TARGETS xrt_aws
        LIBRARY
        DESTINATION ${XRT_INSTALL_DIR}/lib
        COMPONENT aws)

endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
