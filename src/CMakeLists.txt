PROJECT(XRT)
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

SET(PROJECT_DESCRIPTION "https://github.com/Xilinx/XRT")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/driver/include
  )

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX "/opt/xilinx")
endif()
set (CMAKE_INSTALL_PREFIX "/opt/xilinx")

# Default component name for any install() command without the COMPONENT argument
set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "xrt")

set (XRT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set (XRT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/xrt")

set(XRT_VERSION_RELEASE 201830)
SET(XRT_VERSION_MAJOR 2)
SET(XRT_VERSION_MINOR 1)

if (DEFINED ENV{XRT_VERSION_PATCH})
  SET(XRT_VERSION_PATCH $ENV{XRT_VERSION_PATCH})
else(DEFINED $ENV{XRT_VERSION_PATCH})
  SET(XRT_VERSION_PATCH 0)
endif(DEFINED ENV{XRT_VERSION_PATCH})

#set(XRT_SOVERSION ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR})
set(XRT_SOVERSION ${XRT_VERSION_MAJOR})
set(XRT_VERSION_STRING ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/")

# Release OpenCL extension headers
set(XRT_CL_EXT_SRC
  include/1_2/CL/cl_ext_xilinx.h
  include/1_2/CL/cl_ext.h)
install (FILES ${XRT_CL_EXT_SRC} DESTINATION ${XRT_INSTALL_DIR}/include/CL)
message("-- XRT CL extension header files")
foreach (header ${XRT_CL_EXT_SRC})
  message("-- ${header}")
endforeach()

# Release eula (EA temporary)
file(GLOB XRT_EULA
  "license/*.txt"
  )
#install (FILES ${XRT_EULA} DESTINATION ${XRT_INSTALL_DIR}/license)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE DESTINATION ${XRT_INSTALL_DIR}/license)
message("-- XRT EA eula files  ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")

include (CMake/version.cmake)

include (CMake/ccache.cmake)

message("-- ${CMAKE_SYSTEM_INFO_FILE} (${LINUX_FLAVOR}) (Kernel ${LINUX_KERNEL_VERSION})")
message("-- Compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_C_COMPILER}")

add_subdirectory(runtime_src)

#XMA settings START
set (XMA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set (XMA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/xrt")
set(XMA_VERSION_STRING ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH})
set(XMA_SOVERSION ${XRT_SOVERSION})
add_subdirectory(xma)
#XMA settings END

message("-- XRT version: ${XRT_VERSION_STRING}")

include (CMake/cpack.cmake)

include (CMake/lint.cmake)

set (XRT_DKMS_DRIVER_SRC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime_src")

include (CMake/dkms.cmake)
include (CMake/dkms-aws.cmake)

include (CMake/icd.cmake)

include (CMake/pkgconfig.cmake)

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  include(CMake/native.cmake)
else()
  include(CMake/embedded_system.cmake)
endif()

