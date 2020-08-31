project(pyxrt)
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

find_package(pybind11 REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(XILINX_XRT $ENV{XILINX_XRT})
set(XRT_CORE_LIBRARY xrt_core)
set(MODE hw)

if (DEFINED ENV{XCL_EMULATION_MODE})
  set(MODE $ENV{XCL_EMULATION_MODE})
  string(REPLACE "_" "" XCL_EMU_SUFFIX ${MODE})
  set(XRT_CORE_LIBRARY xrt_${XCL_EMU_SUFFIX})
  message("=============== XRT_CORE_LIB=${XRT_CORE_LIBRARY}")
endif()

find_library(xrt_coreutil_LIBRARY
  NAMES xrt_coreutil
  HINTS "${XILINX_XRT}/lib")
message("xrt_coreutil_LIBRARY=${xrt_coreutil_LIBRARY}")

find_library(xrt_xilinxopencl_LIBRARY
  NAMES xilinxopencl
  HINTS "${XILINX_XRT}/lib")
message("xrt_xilinxopencl_LIBRARY=${xrt_xilinxopencl_LIBRARY}")

if (NOT WIN32)
find_library(uuid_LIBRARY
  NAMES uuid)
message("uuid_LIBRARY=${uuid_LIBRARY}")
endif (NOT WIN32)

include_directories(${XILINX_XRT}/include)

pybind11_add_module(pyxrt src/pyxrt.cpp)
target_link_libraries(pyxrt PRIVATE ${xrt_coreutil_LIBRARY})
target_link_libraries(pyxrt PRIVATE ${uuid_LIBRARY} pthread)
