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

target_link_libraries(awssak
  xrt-awsstatic
  pthread
  rt
  )

install (TARGETS awssak RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

