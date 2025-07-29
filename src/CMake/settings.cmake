# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif (NOT CMAKE_CXX_STANDARD)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("-- Host system processor is ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message("-- Target system processor is ${CMAKE_SYSTEM_PROCESSOR}")

# Indicate that we are building XRT
add_compile_definitions("XRT_BUILD")

# Note if we are building for upstream packaging
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

if (DISABLE_ABI_CHECK)
  add_compile_options("-DDISABLE_ABI_CHECK")
endif()

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

# --- version settings ---
# Version adjusted to 2.20 for 2025.2
set(XRT_VERSION_RELEASE 202520)
SET(XRT_VERSION_MAJOR 2)
SET(XRT_VERSION_MINOR 20)

if (DEFINED ENV{XRT_VERSION_PATCH})
  SET(XRT_VERSION_PATCH $ENV{XRT_VERSION_PATCH})
else(DEFINED $ENV{XRT_VERSION_PATCH})
  SET(XRT_VERSION_PATCH 0)
endif(DEFINED ENV{XRT_VERSION_PATCH})

# Also update cache to set version for external plug-in .so
set(XRT_SOVERSION ${XRT_VERSION_MAJOR} CACHE INTERNAL "")
set(XRT_VERSION_STRING ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH} CACHE INTERNAL "")
