# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

# Note: Add Windows compilation directives at the END of this file

if( NOT WIN32)

if(${CMAKE_BUILD_TYPE} STREQUAL "Release" AND (NOT XRT_EDGE))
  xrt_add_subdirectory(doc)
endif()

option(XOCL_VERBOSE "Enable xocl verbosity" OFF)
option(XRT_VERBOSE "Enable xrt verbosity" OFF)

if (XOCL_VERBOSE)
  add_compile_options("-DXOCL_VERBOSE")
endif()

if(XRT_VERBOSE)
  add_compile_options("-DXRT_VERBOSE")
endif()

add_compile_options("-fPIC")

if (XRT_ENABLE_WERROR)
  list(APPEND XRT_WARN_OPTS "-Werror")
endif(XRT_ENABLE_WERROR)

if (NOT XRT_EDGE)
  add_compile_options( ${XRT_WARN_OPTS} )
endif()
xrt_add_subdirectory(xdp)

xrt_add_subdirectory(tools/xclbinutil)
xrt_add_subdirectory(xocl)
xrt_add_subdirectory(xrt)

if (XRT_ALVEO)
  # Enable building of ERT firmware for Alveo
  xrt_add_subdirectory(ert)
endif()

# --- Optional HIP bindings ---
if (XRT_ENABLE_HIP)
  xrt_add_subdirectory(hip)
endif(XRT_ENABLE_HIP)

if (NOT XRT_EDGE)
  # Only for host native build.
  # For embedded, headers and libraries are installed in /usr
  # Not requeired setup.sh/.csh
  xrt_add_subdirectory(tools/scripts)
  if (XRT_NPU)
    # Used by xdp for include search path
    set(AIERT_DIR ${CMAKE_CURRENT_BINARY_DIR}/aie-rt/driver/src)
    set(AIEBU_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core/common/aiebu)
    set(XAIENGINE_BUILD_SHARED OFF)
    xrt_add_subdirectory_disable_install_target(aie-rt/driver/src)
  endif()

endif()

# Attach to the user's linker flags
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")

xrt_add_subdirectory(core)

else()
# = WINDOWS ===================================================================

  # Enable ALL warnings and make them errors if they occur
  if (MSVC)
    # warning level 4 and all warnings are errors
    add_compile_options(/WX /W4)
    if (DEFINED MSVC_PARALLEL_JOBS)
      string(CONCAT MP_OPTION "/MP" ${MSVC_PARALLEL_JOBS})
      add_compile_options(${MP_OPTION})
    endif()
  else()
    # lots of warnings and all warnings as errors
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
    add_compile_options( ${XRT_WARN_OPTS} )
  endif()

  if (XRT_NPU)
    # Used by xdp for include search path
    set(AIERT_DIR ${CMAKE_CURRENT_BINARY_DIR}/aie-rt/driver/src)
    set(AIEBU_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core/common/aiebu)
    set(XAIENGINE_BUILD_SHARED OFF)
    xrt_add_subdirectory_disable_install_target(aie-rt/driver/src)
  endif()

  # Build Subdirectories
  xrt_add_subdirectory(xocl)
  xrt_add_subdirectory(xrt)
  xrt_add_subdirectory(tools/xclbinutil)
  xrt_add_subdirectory(xdp)
  xrt_add_subdirectory(tools/scripts)
  xrt_add_subdirectory(core)

  # --- Optional HIP bindings ---
  if (XRT_ENABLE_HIP)
    xrt_add_subdirectory(hip)
  endif(XRT_ENABLE_HIP)
endif()
