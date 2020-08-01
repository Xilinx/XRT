file(GLOB XMA_API_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

set(XMA_API_ALL_SRC
  ${XMA_API_FILES}
  )

add_library(xma2api SHARED ${XMA_API_ALL_SRC})
target_include_directories(xma2api PRIVATE
  "${OPENCL_INCLUDEDIR}")

set_target_properties(xma2api PROPERTIES VERSION ${XMA_VERSION_STRING}
  SOVERSION ${XMA_SOVERSION})

#removed xrt_core lib as name is different for aws and xbb
target_link_libraries(xma2api
  m
  dl
  gcc_s
  stdc++
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

install(TARGETS xma2api LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib)
