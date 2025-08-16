# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
CMAKE_MINIMUM_REQUIRED(VERSION 3.18.0)
PROJECT(common-test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (MSVC)
  add_compile_options(/Zc:__cplusplus)

  add_compile_options(
    /MT$<$<CONFIG:Debug>:d>  # static linking with the CRT
    /Zc:__cplusplus
    /Zi           # generate pdb files even in release mode
    /sdl          # enable security checks
    /Qspectre     # compile with the Spectre mitigations switch
    /ZH:SHA_256   # enable secure source code hashing
    /guard:cf     # enable compiler control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    )
  add_link_options(
    /NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib  # Hybrid CRT
    /DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib       # Hybrid CRT
    /DEBUG      # instruct linker to create debugging info
    /guard:cf   # enable linker control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    )
endif()

find_package(XRT REQUIRED HINTS ${XILINX_XRT}/share/cmake/XRT)
message("-- XRT_INCLUDE_DIRS=${XRT_INCLUDE_DIRS}")

add_executable(archive archive.cpp)
target_include_directories(archive PRIVATE
  ${XRT_INCLUDE_DIRS}
  # path to runtime_src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
target_link_libraries(archive PRIVATE XRT::xrt_coreutil)

if (NOT MSVC)
  target_link_libraries(archive PRIVATE pthread uuid dl)
endif()

install(TARGETS archive)

