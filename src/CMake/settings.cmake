# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif (NOT CMAKE_CXX_STANDARD)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

message("-- Host system processor is ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message("-- Target system processor is ${CMAKE_SYSTEM_PROCESSOR}")

# Indicate that we are building XRT
add_compile_definitions("XRT_BUILD")

# Note if we are building for upstream packaging
set (XRT_UPSTREAM 0)
if (XRT_UPSTREAM_DEBIAN)
  set (XRT_UPSTREAM 1)
endif()

find_package(Git)

if (GIT_FOUND)
  message(STATUS "-- GIT found: ${GIT_EXECUTABLE}")
elseif (XRT_UPSTREAM)
  message(STATUS "-- GIT not found, ignored for upstream build")
else()
  message(FATAL_ERROR "-- GIT not found, required for internal build")
endif()

if ( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" )
  set(XRT_WARN_OPTS
  -Wall
  -Wno-mismatched-tags
  -Wno-unused-const-variable
  -Wno-unused-private-field
  -Wno-missing-braces
  -Wno-self-assign
  -Wno-pessimizing-move
  -Wno-unused-function
  -Wno-unused-variable
  -Wno-parentheses
  # These are dangerous and should be reviewed
  -Wno-overloaded-virtual
   )
else()
  set(XRT_WARN_OPTS -Wall)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

# --- version settings ---
# Version adjusted to 2.21 for 2026.1
set(XRT_VERSION_RELEASE 202610)
set(XRT_VERSION_MAJOR 2)
set(XRT_VERSION_MINOR 21)

# Upstream builds cannot set XRT_VERSION_PATCH directory as it is
# reset by project(xrt).  Instead upstream builds sets
# XRT_BUILD_NUMBER which is then used here
if (XRT_BUILD_NUMBER)
   set(XRT_VERSION_PATCH ${XRT_BUILD_NUMBER})
elseif (DEFINED ENV{XRT_VERSION_PATCH})
  set(XRT_VERSION_PATCH $ENV{XRT_VERSION_PATCH})
else()
  set(XRT_VERSION_PATCH 0)
endif()

# Also update cache to set version for external plug-in .so
set(XRT_SOVERSION ${XRT_VERSION_MAJOR} CACHE INTERNAL "")
set(XRT_VERSION_STRING ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH} CACHE INTERNAL "")
