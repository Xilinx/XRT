# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.10...4.0)
if (POLICY CMP0144)
  message("--using CMP0144 for Protobuf checking")
  cmake_policy(SET CMP0144 NEW)
else (POLICY CMP0144)
  message("--CMP0144 is not supported, skipping xbtracer")
  return()
endif ()

find_package(Protobuf)
if (NOT Protobuf_FOUND)
  message("Protobuf is not found, skipping xbtracer")
  return()
endif (NOT Protobuf_FOUND)
message("Protobuf version is ${Protobuf_VERSION}.")
if (Protobuf_VERSION VERSION_LESS 3.0)
  # we use timestamp feature of protobuf
  message("Protobuf version ${Protobuf_VERSION} is less than 3.0, skip xbtracer.")
  return()
endif (Protobuf_VERSION VERSION_LESS 3.0)

include_directories (
${Protobuf_INCLUDE_DIR}
${CMAKE_CURRENT_BINARY_DIR}
)
# Generate Cpp files from Proto file
file(GLOB PROTO_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.proto"
)

PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${PROTO_SRC_FILES})

add_custom_target(xbtracer_generated_code DEPENDS ${ProtoSources} ${ProtoHeaders})

add_library(xbtracer_protobuf STATIC ${ProtoSources} ${ProtoHeaders})
add_dependencies(xbtracer_protobuf xbtracer_generated_code)
if (MSVC)
  target_compile_options(xbtracer_protobuf PRIVATE /wd4244 /wd4267)
endif(MSVC)
