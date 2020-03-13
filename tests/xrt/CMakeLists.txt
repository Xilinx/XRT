CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(XILINX_XRT $ENV{XILINX_XRT})
set(XRT_CORE_LIBRARY xrt_core)

if (DEFINED ENV{XCL_EMULATION_MODE})
  string(REPLACE "_" "" XCL_EMU_SUFFIX $ENV{XCL_EMULATION_MODE})
  set(XRT_CORE_LIBRARY xrt_${XCL_EMU_SUFFIX})
  message("=============== XRT_CORE_LIB=${XRT_CORE_LIBRARY}")
endif()

find_library(xrt_core_LIBRARY
  NAMES ${XRT_CORE_LIBRARY}
  HINTS "${XILINX_XRT}/lib")
message("xrt_core_LIBRARY=${xrt_core_LIBRARY}")

find_library(xrt_coreutil_LIBRARY
  NAMES xrt_coreutil
  HINTS "${XILINX_XRT}/lib")
message("xrt_coreutil_LIBRARY=${xrt_coreutil_LIBRARY}")

find_library(xrt_xilinxopencl_LIBRARY
  NAMES xilinxopencl
  HINTS "${XILINX_XRT}/lib")
message("xrt_xilinxopencl_LIBRARY=${xrt_xilinxopencl_LIBRARY}")

find_library(uuid_LIBRARY
  NAMES uuid)

message("uuid_LIBRARY=${uuid_LIBRARY}")

include_directories(${XILINX_XRT}/include)

if (NOT DEFINED ${INSTALL_DIR})
  set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}")
endif()

add_subdirectory(100_ert_ncu)
