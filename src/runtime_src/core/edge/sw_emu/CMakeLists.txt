# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generic_pcie_hal2")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../common_em")
set(COMMON_EM_GEN_DIR  "${CMAKE_CURRENT_BINARY_DIR}/../common_em")
set(COMMON_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(Protobuf_DEBUG)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

include_directories(
  ${EM_SRC_DIR}
  ${COMMON_SRC_DIR}
  ${COMMON_EM_SRC_DIR}
  ${COMMON_EM_GEN_DIR}
  )


file(GLOB EM_SRC_FILES
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cxx"
  "${COMMON_SRC_DIR}/common/*.h"
  "${COMMON_SRC_DIR}/common/*.cpp"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)

add_library(sw_emu_objects OBJECT ${EM_SRC_FILES})
add_dependencies(sw_emu_objects edge_emulation_generated_code)

set(CURR_SOURCE "")
add_library(xrt_swemu SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sw_emu_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_swemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_swemu
  PRIVATE
  ${PROTOBUF_LIBRARY}
  xrt_coreutil
  common_em
  pthread
  rt
  uuid
  )

install (TARGETS xrt_swemu 
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}
)
