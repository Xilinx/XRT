if(${INTERNAL_TESTING})
  add_definitions(-DINTERNAL_TESTING)
else()
  include_directories(../../../../../../../sdk/userspace/include/)
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

set(XRT_AWS_SRC
  ${XRT_USER_AWS_FILES}
  )

add_library(user_aws OBJECT ${XRT_AWS_SRC})
