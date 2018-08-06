include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/user_gem/
  )

set(XRT_SRC "")

add_library(xrt_core SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_gem>
  )

add_library(xrt_corestatic STATIC ${XRT_SRC}
  $<TARGET_OBJECTS:user_common>
  $<TARGET_OBJECTS:user_gem>
  )

set_target_properties(xrt_core PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC" "-fvisibility=hidden")
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(user_common)
add_subdirectory(user_gem)

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_VERSION_MAJOR})

target_link_libraries(xrt_core
  pthread
  rt
  )

install (TARGETS xrt_core LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
