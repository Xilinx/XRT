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

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ${XRT_BINARY_DIR}/gen
)

file(GLOB XBTRACER_COMMON_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp"
)

add_library(xbtracer_common STATIC ${XBTRACER_COMMON_SRC_FILES})
if (NOT WIN32)
  target_link_libraries(xbtracer_common PRIVATE dl)
endif (NOT WIN32)

file(GLOB XBTRACER_WRAPPER_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/wrapper/*.cpp"
)

add_library(xrt_wrapper SHARED ${XBTRACER_WRAPPER_SRC_FILES} ${ProtoHeaders})

target_link_libraries(xrt_wrapper PRIVATE xbtracer_common xbtracer_protobuf ${Protobuf_LIBRARIES} xrt_coreutil)
add_dependencies(xrt_wrapper xbtracer_common xbtracer_protobuf xrt_coreutil)

file(GLOB XBTRACER_CAPTURE_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/capture/*.cpp"
)

add_executable(xbtracer ${XBTRACER_CAPTURE_SRC_FILES})
target_link_libraries(xbtracer PRIVATE xbtracer_common)
add_dependencies(xbtracer xbtracer_common)

file(GLOB XBREPLAY_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/replay/*.cpp"
)
add_executable(xbreplay ${XBREPLAY_SRC_FILES})
target_link_libraries(xbreplay PRIVATE xbtracer_common xbtracer_protobuf ${Protobuf_LIBRARIES} xrt_coreutil)
if (NOT WIN32)
  target_link_libraries(xbreplay PRIVATE pthread)
endif (NOT WIN32)
add_dependencies(xbreplay xbtracer_common xbtracer_protobuf xrt_coreutil)

if (WIN32)
include (detoursUtil.cmake)

target_include_directories(xrt_wrapper PRIVATE ${DETOURS_INCLUDE})
target_link_libraries(xrt_wrapper PRIVATE ms_detours)
add_dependencies(xrt_wrapper ms_detours)
target_include_directories(xbtracer PRIVATE ${DETOURS_INCLUDE})
target_link_libraries(xbtracer PRIVATE ms_detours)
add_dependencies(xbtracer ms_detours)
endif(WIN32)

# Install our built executable
set(XBTRACER_TARGETS xbtracer xbreplay)
install (TARGETS ${XBTRACER_TARGETS}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
set (XBTRACER_HELPER_SCRIPTS xbtracer.bat xbreplay.bat)
install (PROGRAMS ${XBTRACER_HELPER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install(TARGETS xrt_wrapper
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT}
)
