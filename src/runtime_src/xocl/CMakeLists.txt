# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${Boost_INCLUDE_DIRS}
  ${XRT_SOURCE_DIR}/include/1_2
)

set(XRT_XOCL_API_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/api")
set(XRT_XOCL_CORE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(XRT_XOCL_XCLBIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/xclbin")

include_directories(
  ${XRT_XOCL_API_DIR}
)

file(GLOB XRT_XOCL_API_FILES
  "${XRT_XOCL_API_DIR}/*.cpp"
  "${XRT_XOCL_API_DIR}/detail/*.cpp"
  "${XRT_XOCL_API_DIR}/icd/*.cpp"
  "${XRT_XOCL_API_DIR}/khronos/*.cpp"
  "${XRT_XOCL_API_DIR}/xlnx/*.cpp"
  "${XRT_XOCL_API_DIR}/printf/*.cpp"
  "${XRT_XOCL_API_DIR}/plugin/xdp/*.cpp"
  )

file(GLOB XRT_XOCL_CORE_FILES
  "${XRT_XOCL_CORE_DIR}/*.cpp"
  )

file(GLOB XRT_XOCL_XCLBIN_FILES
  "${XRT_XOCL_XCLBIN_DIR}/*.cpp"
  )

set(XRT_XOCL_ALL_SRC
  ${XRT_XOCL_API_FILES}
  ${XRT_XOCL_CORE_FILES}
  ${XRT_XOCL_XCLBIN_FILES}
  )

add_compile_options("-DXRT_XOCL_SOURCE")

add_library(xocl OBJECT ${XRT_XOCL_ALL_SRC})

if (WIN32)
include_directories(
  ${KHRONOS}/include
  )

add_library(xilinxopencl SHARED
  $<TARGET_OBJECTS:xocl>
  ${XRT_XOCL_API_DIR}/icd/windows/xilinxopencl.def
  )

add_library(xilinxopencl_static STATIC
  $<TARGET_OBJECTS:xocl>
  )

target_link_libraries(xilinxopencl
  PRIVATE
  xrt++
  xrt_coreutil
  )

else ()

add_library(xilinxopencl SHARED $<TARGET_OBJECTS:xocl>)
add_library(xilinxopencl_static STATIC $<TARGET_OBJECTS:xocl>)

# Attach to the user's linker flags
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")

set_target_properties(xilinxopencl PROPERTIES
  LINKER_LANGUAGE CXX
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

target_link_libraries(xilinxopencl
  PRIVATE
  xrt++
  xrt_coreutil
  dl
  pthread
  crypt
  uuid
  rt
  )

endif ()

install(TARGETS xilinxopencl xilinxopencl_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)

# Release OpenCL extension headers
install (FILES
  ${XRT_SOURCE_DIR}/include/1_2/CL/cl_ext_xilinx.h
  ${XRT_SOURCE_DIR}/include/1_2/CL/cl_ext.h
  ${XRT_SOURCE_DIR}/include/1_2/CL/cl2xrt.hpp
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/CL
  COMPONENT ${XRT_BASE_DEV_COMPONENT}
)
