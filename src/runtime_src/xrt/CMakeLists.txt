# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
add_subdirectory(xrt++)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT WIN32)
  include_directories(${DRM_INCLUDE_DIRS})
endif()

set(XRT_XRT_DEVICE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/device")
set(XRT_XRT_UTIL_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/util")
set(XRT_XRT_CPP_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/xrt++")

file(GLOB XRT_XRT_ALL_SRC_SHARED
  "${XRT_XRT_DEVICE_DIR}/device.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal2.cpp"
  "${XRT_XRT_UTIL_DIR}/*.cpp"
  "${XRT_XRT_CPP_DIR}/*.cpp"
  )

file(GLOB XRT_XRT_ALL_SRC_STATIC
  "${XRT_XRT_DEVICE_DIR}/device.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal_static.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal2.cpp"
  "${XRT_XRT_UTIL_DIR}/*.cpp"
  "${XRT_XRT_CPP_DIR}/*.cpp"
  )

add_compile_options("-DXRT_SOURCE")

if (NOT WIN32)
set_source_files_properties(${XRT_XRT_DEVICE_DIR}/hal_static.cpp PROPERTIES
  COMPILE_FLAGS -Wno-unused
  )
endif()

add_library(xrt++ SHARED
  ${XRT_XRT_ALL_SRC_SHARED}
  )

add_library(xrt++_static STATIC
  ${XRT_XRT_ALL_SRC_STATIC}
  )

target_link_libraries(xrt++
  PRIVATE
  xrt_coreutil
  ${Boost_SYSTEM_LIBRARY}
  )

set_target_properties(xrt++ PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

install(TARGETS xrt++ xrt++_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)
