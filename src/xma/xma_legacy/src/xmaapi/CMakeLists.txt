file(GLOB XMA_API_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

set(XMA_API_ALL_SRC
  ${XMA_API_FILES}
  )

add_library(xmaapi SHARED ${XMA_API_ALL_SRC})
target_include_directories(xmaapi PRIVATE
  "${OPENCL_INCLUDEDIR}"
)

set_target_properties(xmaapi PROPERTIES VERSION ${XMA_VERSION_STRING}
  SOVERSION ${XMA_SOVERSION})

#removed xrt_core lib as name is different for aws and xbb
target_link_libraries(xmaapi
  m
  dl
  gcc_s
  stdc++
  yaml
  )


install(TARGETS xmaapi
  LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XMA_INSTALL_DIR}/bin
)

install(TARGETS xmaapi
  ARCHIVE DESTINATION ${XMA_INSTALL_DIR}/lib COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
