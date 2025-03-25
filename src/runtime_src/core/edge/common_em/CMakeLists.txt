# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
SET (Protobuf_DEBUG)
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

include_directories (
${PROTOBUF_INCLUDE_DIR}
)
# Generate Cpp files from Proto file
file(GLOB PROTO_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.proto"
  )

PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${PROTO_SRC_FILES})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_custom_target(edge_emulation_generated_code DEPENDS ${ProtoSources} ${ProtoHeaders})
file(GLOB COMMON_EM_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)

add_library(common_em_objects OBJECT ${COMMON_EM_SRC_FILES} ${ProtoHeaders} ${ProtoSources})

set(CURR_SOURCE "")
add_library(common_em STATIC ${CURR_SOURCE}
  $<TARGET_OBJECTS:common_em_objects>
  )

add_dependencies(common_em edge_emulation_generated_code)

set_target_properties(common_em PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(common_em
  PRIVATE
  ${PROTOBUF_LIBRARY}
  xrt_coreutil
  dl
  pthread
  crypt
  rt
  )

