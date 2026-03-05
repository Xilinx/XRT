# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#
# Generic Protobuf setup: find Protobuf (CONFIG or MODULE), resolve
# protoc, and provide a function to generate C++ from .proto files.

# Set CMP0144 early for consistent ROOT variable behavior
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# Now Protobuf_ROOT works reliably across CMake versions
set(Protobuf_ROOT "" CACHE PATH "Path to Protobuf installation")

# Prefer config mode for Protobuf, fallback to module mode.
# Do not use REQUIRED so callers can skip (e.g. xbtracer) when Protobuf is not
# available (common on Windows if Protobuf is not installed or not in PATH).
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
find_package(Protobuf QUIET)
unset(CMAKE_FIND_PACKAGE_PREFER_CONFIG)

if(NOT Protobuf_FOUND)
  return()
endif()

# Check if we're in config mode and enable module-compatible functions if needed
if(TARGET protobuf::libprotobuf)
  set(PROTOBUF_CONFIG_MODE TRUE)
  set(protobuf_MODULE_COMPATIBLE TRUE)
  message(STATUS "Protobuf found in CONFIG mode")

  # CRITICAL: Ensure protobuf::protoc exists for Yocto cross-compilation
  if(NOT TARGET protobuf::protoc)
    if(NOT Protobuf_PROTOC_EXECUTABLE)
      find_program(Protobuf_PROTOC_EXECUTABLE protoc 
        PATHS ${CMAKE_FIND_ROOT_PATH}/usr/bin 
        NO_DEFAULT_PATH
      )
    endif()
    if(Protobuf_PROTOC_EXECUTABLE)
      add_executable(protobuf::protoc IMPORTED)
      set_target_properties(protobuf::protoc PROPERTIES
        IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}"
      )
      message(STATUS "Created protobuf::protoc target: ${Protobuf_PROTOC_EXECUTABLE}")
    endif()
  endif()
elseif(Protobuf_LIBRARIES)
  set(PROTOBUF_CONFIG_MODE FALSE)
  add_library(protobuf::libprotobuf UNKNOWN IMPORTED)
  set_target_properties(protobuf::libprotobuf PROPERTIES
    IMPORTED_LOCATION "${Protobuf_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}"
    INTERFACE_COMPILE_DEFINITIONS "${Protobuf_DEFINITIONS}"
  )
  if(Protobuf_PROTOC_EXECUTABLE)
    add_executable(protobuf::protoc IMPORTED)
    set_target_properties(protobuf::protoc PROPERTIES
      IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}"
    )
  endif()
  message(STATUS "Protobuf found in MODULE mode - created compatibility targets")
else()
  set(Protobuf_FOUND FALSE)
  return()
endif()

# Set global compatibility flag for config mode (enables legacy functions)
set(protobuf_MODULE_COMPATIBLE TRUE CACHE BOOL "Enable module-compatible functions in config mode" FORCE)

# Function to generate C++ headers and sources from .proto files
# Usage: protobuf_generate_cpp(<target> <proto_files>...)
# Adds generated sources/headers to <target> and depends on them
function(protobuf_generate_cpp target)
  if(NOT Protobuf_FOUND)
    message(FATAL_ERROR "Protobuf not found")
  endif()

  set(proto_files ${ARGN})
  if(NOT proto_files)
    message(FATAL_ERROR "No proto files provided to protobuf_generate_cpp")
  endif()

  if(PROTOBUF_CONFIG_MODE)
    # Use modern protobuf_generate with TARGET
    message("-- Use modern protobuf_generate with TARGET")
    protobuf_generate(
      TARGET ${target}
      PROTOS ${proto_files}
      LANGUAGE cpp
    )
  else()
    # Module mode: use legacy protobuf_generate_cpp
    message("-- Module mode: use legacy protobuf_generate_cpp")
    set(proto_sources)
    set(proto_headers)
    protobuf_generate_cpp(
      proto_sources
      proto_headers
      ${proto_files}
    )
    target_sources(${target} PRIVATE ${proto_sources} ${proto_headers})
  endif()
endfunction()
