# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generic_pcie_hal2")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../common_em")
set(COMMON_EM_GEN_DIR  "${CMAKE_CURRENT_BINARY_DIR}/../common_em")
set(COMMON_PCIE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")

include_directories(
  ${EM_SRC_DIR}
  ${COMMON_EM_SRC_DIR}
  ${COMMON_EM_GEN_DIR}
  )

file(GLOB EM_SRC_FILES
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cxx"
  "${COMMON_PCIE_SRC_DIR}/system_pcie.cpp"
  "${COMMON_PCIE_SRC_DIR}/device_pcie.cpp"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(sw_emu_objects OBJECT ${EM_SRC_FILES})
add_dependencies(sw_emu_objects pcie_emulation_generated_code)

set(CURR_SOURCE "")
add_library(xrt_swemu SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sw_emu_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

add_library(xrt_swemu_static STATIC ${CURR_SOURCE}
  $<TARGET_OBJECTS:sw_emu_objects>
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:common_em_objects>
  )

set_target_properties(xrt_swemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_swemu
  PRIVATE
  ${PROTOBUF_LIBRARY}
  xrt_coreutil
  common_em
  rt
  uuid
  )

target_link_libraries(xrt_swemu_static
  INTERFACE
  ${PROTOBUF_LIBRARY}
  xrt_coreutil_static
  rt
  uuid
  )

install (TARGETS xrt_swemu xrt_swemu_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_COMPONENT} NAMELINK_COMPONENT ${XRT_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
)
