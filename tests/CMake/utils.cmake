# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Xilinx, Inc. All rights reserved.
#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(XILINX_XRT $ENV{XILINX_XRT})
set(XRT_CORE_LIBRARY xrt_core)
set(XILINX_VITIS $ENV{XILINX_VITIS})

if(NOT VITIS_COMPILER)
  find_program(VITIS_COMPILER
  NAMES v++
  HINTS $ENV{XILINX_VITIS}
  )
  if(VITIS_COMPILER)
    message("Vitis Compiler is found at ${VITIS_COMPILER}")
  endif()
endif()

if(NOT XRT)
  set(XRT_DIR ${XILINX_XRT}/share/cmake/XRT)
  find_package(XRT)
endif()

set(xrt_core_LIBRARY XRT::xrt_core)
set(xrt_coreutil_LIBRARY XRT::xrt_coreutil)
set(xrt_xilinxopencl_LIBRARY XRT::xilinxopencl)
set(xrt_core_static_LIBRARY XRT::xrt_core_static)
set(xrt_coreutil_static_LIBRARY XRT::xrt_coreutil_static)
set(xrt_xilinxopencl_static_LIBRARY XRT::xilinxopencl_static)
set(xrt_hip_LIBRARY XRT::xrt_hip)

if (DEFINED ENV{XCL_EMULATION_MODE})
    set(MODE $ENV{XCL_EMULATION_MODE})
    string(REPLACE "_" "" XCL_EMU_SUFFIX ${MODE})
    set(xrt_core_LIBRARY XRT::xrt_${XCL_EMU_SUFFIX})
endif()

include_directories(${XRT_INCLUDE_DIRS})

if (NOT WIN32)
  if(NOT DEFINED uuid_LIBRARY)
    find_library(uuid_LIBRARY
    NAMES uuid)
    message("uuid_LIBRARY=${uuid_LIBRARY}")
  endif()
else()
    set(BOOST_ROOT C:/Xilinx/XRT/ext)
    if(CMAKE_VERSION VERSION_GREATER "3.29")
      find_package(Boost CONFIG)
    else (CMAKE_VERSION VERSION_GREATER "3.29")
      find_package(Boost)
    endif (CMAKE_VERSION VERSION_GREATER "3.29")
    include_directories(${Boost_INCLUDE_DIRS})
endif(NOT WIN32)

if (NOT DEFINED ${INSTALL_DIR})
  set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}/${MODE}")
endif()

# function for creation emconfig.json file for sw_emu, hw_emu
function(xrt_create_emconfig PLATFORM)
  add_custom_command(
    OUTPUT emconfig.json
    COMMAND emconfigutil
    --platform ${PLATFORM}
    --nd 1
  )
  add_custom_target(EMCONFIG_${TESTNAME}
  ALL
  DEPENDS emconfig.json
  )
endfunction()

# macro to generate XO file
# xrt_create_xo macro has two arguments, SRC, OUT
# SRC                     Path to kernel code (.cl | .cpp)
# COMPILE_OPTIONS   Any extra compile options to be provided for xo generation
# OUT                     Name of the .xo to be generated
macro(xrt_create_xo SRC COMPILE_OPTIONS OUT)
  set(OUT_FILE "${OUT}_${MODE}")
  set(OUT_TARGET "${OUT_FILE}_${TESTNAME}")
  list(APPEND XOS "${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}.xo")
  list(APPEND XO_TARGETS "${OUT_TARGET}")

  add_custom_command(
    OUTPUT ${OUT_FILE}.xo
    COMMAND ${VITIS_COMPILER}
    -c ${SRC}
    ${COMPILE_OPTIONS}
    -o ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}.xo
    --platform ${PLATFORM}
    -t ${MODE}
  )
  add_custom_target(${OUT_TARGET}
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}.xo
  )
endmacro()

# macro to generate xclbin file
# xrt_create_xclbin() macro has two arguments, LINK_OPTIONS, OUT
# OUT             Name of the xclbin to be generated
# LINK_OPTIONS    Any extra link options while generating xclbin to be provided
macro(xrt_create_xclbin OUT LINK_OPTIONS)

  set(OUT_FILE "${OUT}_${MODE}")
  set(OUT_TARGET "XCLBIN_${OUT_FILE}_${TESTNAME}")

  # custom command to generate .xclbin from previously generated .xo file
  add_custom_command(
    OUTPUT ${OUT_FILE}.xclbin
    COMMAND ${VITIS_COMPILER}
    -l ${XOS}
    ${LINK_OPTIONS}
    -o ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}.xclbin
    --platform ${PLATFORM}
    -t ${MODE}
  )
  add_custom_target(${OUT_TARGET}
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}.xclbin
  )
  add_dependencies(${OUT_TARGET} ${XO_TARGETS})
endmacro()
