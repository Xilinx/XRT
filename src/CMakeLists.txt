PROJECT(SDAccel)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

SET(PROJECT_DESCRIPTION "https://github.com/Xilinx/XRT")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/1_2
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/driver/include
  )

INCLUDE (FindPkgConfig)

# DRM
pkg_check_modules(DRM REQUIRED libdrm)
IF(DRM_FOUND)
  MESSAGE(STATUS "Looking for DRM - found at ${DRM_PREFIX} ${DRM_VERSION}")
  INCLUDE_DIRECTORIES(${DRM_INCLUDEDIR})
ELSE(DRM_FOUND)
  MESSAGE(FATAL_ERROR "Looking for DRM - not found")
ENDIF(DRM_FOUND)

# OpenCL header files
pkg_check_modules(OPENCL REQUIRED OpenCL)
IF(OPENCL_FOUND)
  MESSAGE(STATUS "Looking for OPENCL - found at ${OPENCL_PREFIX} ${OPENCL_VERSION} ${OPENCL_INCLUDEDIR}")
  INCLUDE_DIRECTORIES(${OPENCL_INCLUDEDIR})
ELSE(OPENCL_FOUND)
  MESSAGE(FATAL_ERROR "Looking for OPENCL - not found")
ENDIF(OPENCL_FOUND)

find_program(LSB_RELEASE lsb_release)
find_program(UNAME uname)

execute_process(COMMAND ${LSB_RELEASE} -is
  OUTPUT_VARIABLE LINUX_FLAVOR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(COMMAND ${LSB_RELEASE} -rs
  OUTPUT_VARIABLE LINUX_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(COMMAND ${UNAME} -r
  OUTPUT_VARIABLE LINUX_KERNEL_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

#set(Boost_DEBUG 1)
INCLUDE (FindBoost)
# On older systems libboost_system.a is not compiled with -fPIC which leads to
# link errors when XRT shared objects try to link with it.
# Static linking with Boost is enabled on Ubuntu 18.04.
if ((${LINUX_FLAVOR} STREQUAL Ubuntu) AND (${LINUX_VERSION} STREQUAL 18.04))
   set(Boost_USE_STATIC_LIBS  ON)
endif()
find_package(Boost REQUIRED COMPONENTS system filesystem )

INCLUDE (FindCurses)
find_package(Curses REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX "/opt/xilinx")
endif()
set (CMAKE_INSTALL_PREFIX "/opt/xilinx")

set (XRT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/xrt")

SET(XRT_VERSION_MAJOR 2)
SET(XRT_VERSION_MINOR 1)
SET(XRT_VERSION_PATCH 0)
set(XRT_VERSION_STRING ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/")

message("-- ${CMAKE_SYSTEM_INFO_FILE} (${LINUX_FLAVOR}) (Kernel ${LINUX_KERNEL_VERSION})")
message("-- Compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_C_COMPILER}")

add_subdirectory(runtime_src)

SET(CPACK_SET_DESTDIR ON)
SET(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")

SET(PACKAGE_KIND "TGZ")
if (${LINUX_FLAVOR} STREQUAL Ubuntu)
  SET(CPACK_GENERATOR "DEB;TGZ")
  SET(PACKAGE_KIND "DEB")
  SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/postinst;${CMAKE_CURRENT_BINARY_DIR}/runtime_src/prerm")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "ocl-icd-opencl-dev (>= 2.2.0), libboost-dev (>=1.58), libboost-filesystem-dev (>=1.58), uuid-dev (>= 2.27.1), dkms (>= 2.2.0), libprotoc-dev (>=2.6.1), protobuf-compiler (>=2.6.1), libncurses5-dev (>=6.0)")
elseif ((${LINUX_FLAVOR} STREQUAL Redhat) OR (${LINUX_FLAVOR} STREQUAL CentOS))
  SET(CPACK_GENERATOR "RPM;TGZ")
  SET(PACKAGE_KIND "RPM")
  SET(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/postinst")
  SET(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/prerm")
  SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local" "/usr/src" "/opt" "/etc/OpenCL" "/etc/OpenCL/vendors" )
  set(CPACK_RPM_PACKAGE_REQUIRES "ocl-icd-devel >= 2.2, boost-devel >= 1.53, boost-filesystem >= 1.53, libuuid-devel >= 2.23.2, dkms >= 2.5.0, protobuf-devel >= 2.5.0, protobuf-compiler >= 2.5.0, ncurses-devel >= 5.9")
else ()
  SET (CPACK_GENERATOR "TGZ")
endif()

message("-- XRT version: ${XRT_VERSION_STRING}")
message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

SET(CPACK_PACKAGE_NAME "XRT")
SET(CPACK_PACKAGE_VENDOR "Xilinx Inc")
SET(CPACK_PACKAGE_CONTACT "sonal.santan@xilinx.com")

INCLUDE(CPack)

include (CMake/lint.cmake)
