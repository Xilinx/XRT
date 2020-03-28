add_subdirectory(xrt++)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT WIN32)
  include_directories(${DRM_INCLUDE_DIRS})
else()
set(Boost_USE_STATIC_LIBS     ON)
set(Boost_USE_MULTITHREADED   ON)
find_package(Boost REQUIRED COMPONENTS system filesystem )
include_directories(${Boost_INCLUDE_DIRS})
endif (NOT WIN32)

set(XRT_XRT_DEVICE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/device")
set(XRT_XRT_SCHED_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/scheduler")
set(XRT_XRT_UTIL_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/util")
set(XRT_XRT_CPP_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/xrt++")

file(GLOB XRT_XRT_DEVICE_FILES
  "${XRT_XRT_DEVICE_DIR}/*.h"
  "${XRT_XRT_DEVICE_DIR}/*.cpp"
  )

file(GLOB XRT_XRT_SCHED_FILES
  "${XRT_XRT_SCHED_DIR}/*.h"
  "${XRT_XRT_SCHED_DIR}/*.cpp"
  )

file(GLOB XRT_XRT_UTIL_FILES
  "${XRT_XRT_UTIL_DIR}/*.h"
  "${XRT_XRT_UTIL_DIR}/*.cpp"
  )

file(GLOB XRT_XRT_CPP_FILES
  "${XRT_XRT_CPP_DIR}/*.hpp"
  "${XRT_XRT_CPP_DIR}/*.cpp"
  )

set(XRT_XRT_ALL_SRC
  ${XRT_XRT_DEVICE_FILES}
  ${XRT_XRT_SCHED_FILES}
  ${XRT_XRT_UTIL_FILES}
  ${XRT_XRT_CPP_FILES}
  )

add_compile_options("-DXRT_SOURCE")

add_library(xrt++ SHARED
  ${XRT_XRT_ALL_SRC}
  )

target_link_libraries(xrt++
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

set_target_properties(xrt++ PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

install(TARGETS xrt++
  ARCHIVE DESTINATION ${XRT_INSTALL_DIR}/lib
  LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib
  RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
