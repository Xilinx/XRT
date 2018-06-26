include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(XRT_SRC "")

add_library(xrtcore SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_gem>
  )

add_library(xrtcorestatic STATIC ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_gem>
  )

add_library(xrt-aws SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_aws>
  )

add_library(xrt-awsstatic STATIC ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_aws>
  )

# Core XRT
set_target_properties(xrtcore PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC" "-fvisibility=hidden")
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(user_common)
add_subdirectory(user_gem)

set_target_properties(xrtcore PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_VERSION_MAJOR})

target_link_libraries(xrtcore
  pthread
  rt
  )

install (TARGETS xrtcore LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

# Vendor - AWS 
set_target_properties(xrt-aws PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC" "-fvisibility=hidden")

#add_subdirectory(user_common)
add_subdirectory(user_aws)

set_target_properties(xrt-aws PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_VERSION_MAJOR})

target_link_libraries(xrt-aws
  pthread
  rt
  )

install (TARGETS xrt-aws LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
