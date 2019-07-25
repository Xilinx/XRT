# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH

# --- Git ---
find_package(Git)

IF(GIT_FOUND)
  MESSAGE(STATUS "Looking for GIT - found at ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

# --- Boost ---
#set(Boost_DEBUG 1)

INCLUDE (FindBoost)

set(Boost_USE_STATIC_LIBS     ON)
set(Boost_USE_STATIC_RUNTIME  ON)
set(Boost_USE_MULTITHREADED   OFF)

find_package(Boost REQUIRED COMPONENTS system filesystem )

# --- XRT Variables ---
set (XRT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/xrt")
set (XRT_INSTALL_INCLUDE_DIR "${XRT_INSTALL_DIR}/include")

# --- Release: OpenCL extension headers ---
set(XRT_CL_EXT_SRC
  include/1_2/CL/cl_ext_xilinx.h
  include/1_2/CL/cl_ext.h)
install (FILES ${XRT_CL_EXT_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/CL)
message("-- XRT CL extension header files")
foreach (header ${XRT_CL_EXT_SRC})
  message("-- ${header}")
endforeach()

# --- Release: eula ---
file(GLOB XRT_EULA
  "license/*.txt"
  )
#install (FILES ${XRT_EULA} DESTINATION ${XRT_INSTALL_DIR}/license)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE DESTINATION ${XRT_INSTALL_DIR}/license)
message("-- XRT EA eula files  ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")


# --- Create Version header and JSON file ---
include (CMake/version.cmake)

add_subdirectory(runtime_src)

