# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH


if(NOT WIN32) # TODO: Add Windows Support
INCLUDE (FindPkgConfig)

endif (NOT WIN32)

# -- DRM --
if(NOT WIN32) # TODO: Add Windows Support

pkg_check_modules(DRM REQUIRED libdrm)
IF(DRM_FOUND)
  MESSAGE(STATUS "Looking for DRM - found at ${DRM_PREFIX} ${DRM_VERSION}")
  INCLUDE_DIRECTORIES(${DRM_INCLUDEDIR})
ELSE(DRM_FOUND)
  MESSAGE(FATAL_ERROR "Looking for DRM - not found")
ENDIF(DRM_FOUND)

endif (NOT WIN32)


# -- OpenCL header files -- 
if(NOT WIN32) # TODO: Add Windows Support
pkg_check_modules(OPENCL REQUIRED OpenCL)
IF(OPENCL_FOUND)
  MESSAGE(STATUS "Looking for OPENCL - found at ${OPENCL_PREFIX} ${OPENCL_VERSION} ${OPENCL_INCLUDEDIR}")
  INCLUDE_DIRECTORIES(${OPENCL_INCLUDEDIR})
ELSE(OPENCL_FOUND)
  MESSAGE(FATAL_ERROR "Looking for OPENCL - not found")
ENDIF(OPENCL_FOUND)

endif (NOT WIN32)

# -- Git --
find_package(Git)

IF(GIT_FOUND)
  MESSAGE(STATUS "Looking for GIT - found at ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

if(NOT WIN32) # TODO: Add Windows Support
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
endif (NOT WIN32)

#set(Boost_DEBUG 1)
INCLUDE (FindBoost)

if(NOT WIN32) # TODO: Add Windows Support
# On older systems libboost_system.a is not compiled with -fPIC which leads to
# link errors when XRT shared objects try to link with it.
# Static linking with Boost is enabled on Ubuntu 18.04.
if ((${LINUX_FLAVOR} STREQUAL Ubuntu) AND (${LINUX_VERSION} STREQUAL 18.04))
   set(Boost_USE_STATIC_LIBS  ON)
endif()
if (${LINUX_FLAVOR} STREQUAL pynqlinux)
   set(Boost_USE_STATIC_LIBS  ON)
endif()
find_package(Boost REQUIRED COMPONENTS system filesystem )

endif (NOT WIN32)

if(NOT WIN32) # TODO: Add Windows Support
INCLUDE (FindCurses)
find_package(Curses REQUIRED)

endif (NOT WIN32)

if(NOT WIN32) # TODO: Add Windows Support

set (XRT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/xrt")
set (XRT_INSTALL_INCLUDE_DIR "${XRT_INSTALL_DIR}/include")

# Release OpenCL extension headers
set(XRT_CL_EXT_SRC
  include/1_2/CL/cl_ext_xilinx.h
  include/1_2/CL/cl_ext.h)
install (FILES ${XRT_CL_EXT_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/CL)
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

# Python bindings
set(PY_INSTALL_DIR "${XRT_INSTALL_DIR}/python")
add_subdirectory(python)


message("-- XRT version: ${XRT_VERSION_STRING}")

include (CMake/cpack.cmake)

include (CMake/lint.cmake)

set (XRT_DKMS_DRIVER_SRC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime_src")
include (CMake/dkms.cmake)
include (CMake/dkms-aws.cmake)

include (CMake/icd.cmake)

include (CMake/changelog.cmake)

include (CMake/pkgconfig.cmake)

include (CMake/coverity.cmake)

set (CTAGS "${CMAKE_SOURCE_DIR}/runtime_src/tools/scripts/tags.sh")
include (CMake/tags.cmake)
endif (NOT WIN32)

