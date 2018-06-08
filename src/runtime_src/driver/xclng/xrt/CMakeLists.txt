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
