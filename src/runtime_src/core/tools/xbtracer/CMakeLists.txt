# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

include (${XRT_SOURCE_DIR}/CMake/protobufUtil.cmake)
if (NOT Protobuf_INCLUDE_DIR)
  message("Protobuf is not found, skipping xbtracer")
  return()
else (NOT Protobuf_INCLUDE_DIR)
  message("Protobuf is found, will build xbtracer")
endif (NOT Protobuf_INCLUDE_DIR)

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
