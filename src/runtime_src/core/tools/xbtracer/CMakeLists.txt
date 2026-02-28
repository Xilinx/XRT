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

# Prefer protobuf's config package.
set(XBTRACER_PROTOBUF_CONFIG_MODE FALSE)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG QUIET)
unset(protobuf_MODULE_COMPATIBLE)
if (Protobuf_FOUND)
  set(XBTRACER_PROTOBUF_CONFIG_MODE TRUE)
else()
  find_package(Protobuf)
endif()
if (NOT Protobuf_FOUND)
  message("Protobuf is not found, skipping xbtracer")
  return()
endif (NOT Protobuf_FOUND)

# Keep manual abseil fallback only for module mode.
if (NOT XBTRACER_PROTOBUF_CONFIG_MODE)
  # Protobuf 22+ uses abseil for logging, need to link against it
  # Note: Protobuf versioning changed - version 22.0 may report as 4.22.0 or 6.22.0
  # We check for abseil linking if protobuf version >= 4.0 (corresponds to protobuf 22+)
  find_package(absl QUIET)
  if (absl_FOUND AND Protobuf_VERSION VERSION_GREATER_EQUAL 4.0)
    set(XBTRACER_ABSL_LIBS absl::log_internal_check_op absl::log_internal_message)
    message(STATUS "Found abseil, will link xbtracer against abseil logging libraries")
  endif()
endif()
message("Protobuf version is ${Protobuf_VERSION}.")
if (Protobuf_VERSION VERSION_LESS 3.0)
  # we use timestamp feature of protobuf
  message("Protobuf version ${Protobuf_VERSION} is less than 3.0, skip xbtracer.")
  return()
endif (Protobuf_VERSION VERSION_LESS 3.0)

# Imported targets should link the .lib (not the .dll) when compiling with MSVC
set(XBTRACER_PROTOBUF_LINK "")
if (TARGET protobuf::libprotobuf)
  set(XBTRACER_PROTOBUF_LINK protobuf::libprotobuf)
elseif (TARGET Protobuf::libprotobuf)
  set(XBTRACER_PROTOBUF_LINK Protobuf::libprotobuf)
else()
  set(XBTRACER_PROTOBUF_LINK ${Protobuf_LIBRARIES})
endif()

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
  # Suppress benign third-party header warnings for generated targets in newer protobuf.
  target_compile_options(xbtracer_protobuf PRIVATE /wd4244 /wd4267 /wd4100 /wd4141 /wd4189)
endif(MSVC)
# Link protobuf static library against abseil if needed
if (XBTRACER_ABSL_LIBS)
  target_link_libraries(xbtracer_protobuf PUBLIC ${XBTRACER_ABSL_LIBS})
endif()

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

add_library(xrt_trace SHARED ${XBTRACER_WRAPPER_SRC_FILES} ${ProtoHeaders})
set_target_properties(xrt_trace PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
target_compile_definitions(xrt_trace PRIVATE XRT_ABI_VERSION=${XRT_VERSION_MAJOR})

target_link_libraries(xrt_trace PRIVATE xbtracer_common xbtracer_protobuf ${XBTRACER_PROTOBUF_LINK} xrt_coreutil)
add_dependencies(xrt_trace xbtracer_common xbtracer_protobuf xrt_coreutil)

file(GLOB XBTRACER_CAPTURE_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/capture/*.cpp"
)

add_executable(xrt-tracer ${XBTRACER_CAPTURE_SRC_FILES})
target_link_libraries(xrt-tracer PRIVATE xbtracer_common)
add_dependencies(xrt-tracer xbtracer_common)

file(GLOB XBREPLAY_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/replay/*.cpp"
)
add_executable(xrt-replay ${XBREPLAY_SRC_FILES})
target_link_libraries(xrt-replay PRIVATE xbtracer_common xbtracer_protobuf ${XBTRACER_PROTOBUF_LINK} xrt_coreutil)
if (NOT WIN32)
  target_link_libraries(xrt-replay PRIVATE pthread)
endif (NOT WIN32)
add_dependencies(xrt-replay xbtracer_common xbtracer_protobuf xrt_coreutil)
# TODO: when buiding with yocto for APU in CI, the status return from message to jason convertion function
# provided from protobuf built from yocto doesn't match the one in the absl library, which results in
# build failure. After fixing this issue in yocto APU build, we can always print message as JSON.
# for now, we by default disable it, as it is not the key feature in the tracer/replay prototype, and replay
# only tries to dump JSON message when it fails to replay a function.
if (XRT_XBTRACER_ENABLE_JSON)
  target_compile_options(xrt-replay PRIVATE XBTRACER_PROTOBUF_HAS_JASON)

  add_executable(xbtracer_dump
    src/misc/xbtracer_dump.cpp
  )
  target_link_libraries(xbtracer_dump PRIVATE xbtracer_common xbtracer_protobuf ${XBTRACER_PROTOBUF_LINK})
  add_dependencies(xbtracer_dump xbtracer_common xbtracer_protobuf xrt_coreutil)
endif (XRT_XBTRACER_ENABLE_JSON)

if (WIN32)
include (detoursUtil.cmake)

target_include_directories(xrt_trace PRIVATE ${DETOURS_INCLUDE})
target_link_libraries(xrt_trace PRIVATE ms_detours)
add_dependencies(xrt_trace ms_detours)
target_include_directories(xrt-tracer PRIVATE ${DETOURS_INCLUDE})
target_link_libraries(xrt-tracer PRIVATE ms_detours)
add_dependencies(xrt-tracer ms_detours)
endif(WIN32)

# Install our built executable
set(XBTRACER_TARGETS xrt-tracer xrt-replay)
install (TARGETS ${XBTRACER_TARGETS}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})

# Do not install loader scripts if installing into system default
if (NOT XRT_INSTALL_BIN_DIR STREQUAL XRT_INSTALL_UNWRAPPED_DIR)
  set (XBTRACER_HELPER_SCRIPTS xrt-tracer.bat xrt-replay.bat)
  install (PROGRAMS ${XBTRACER_HELPER_SCRIPTS}
    DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})
endif()

install(TARGETS xrt_trace
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_SKIP
)
