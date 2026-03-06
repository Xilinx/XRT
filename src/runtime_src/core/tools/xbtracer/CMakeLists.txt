# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.10...4.0)

include(${XRT_SOURCE_DIR}/CMake/protobuf.cmake)
if (NOT Protobuf_FOUND)
  message("Protobuf was not found, skip xbtracer.")
  return()
endif ()
if (Protobuf_VERSION VERSION_LESS 3.0)
  message("Protobuf ${Protobuf_VERSION} < 3.0, skip xbtracer.")
  return()
endif ()

################################################################
# Generate C++ from .proto and create xbtracer proto library
################################################################
set(XBTRACER_PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/func.proto)
add_library(xbtracer_protobuf STATIC)
protobuf_generate_cpp(xbtracer_protobuf ${XBTRACER_PROTO_FILES})
target_include_directories(xbtracer_protobuf PRIVATE
  ${Protobuf_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/src
  )
if (MSVC)
  target_compile_options(xbtracer_protobuf PRIVATE /wd4244 /wd4267 /wd4100 /wd4141 /wd4189)
endif ()
target_link_libraries(xbtracer_protobuf PUBLIC protobuf::libprotobuf)

################################################################
# Create xbtracer common library
################################################################
file(GLOB XBTRACER_COMMON_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp)
add_library(xbtracer_common STATIC ${XBTRACER_COMMON_SRC_FILES})
target_include_directories(xbtracer_common PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
if (NOT WIN32)
  target_link_libraries(xbtracer_common PRIVATE dl)
endif (NOT WIN32)

################################################################
# Create xbtracer wrapper library
################################################################
file(GLOB XBTRACER_WRAPPER_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/wrapper/*.cpp)
add_library(xrt_trace SHARED ${XBTRACER_WRAPPER_SRC_FILES})
set_target_properties(xrt_trace PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
target_compile_definitions(xrt_trace PRIVATE XRT_ABI_VERSION=${XRT_VERSION_MAJOR})
target_include_directories(xrt_trace PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/src
  )
target_link_libraries(xrt_trace PRIVATE xbtracer_common xbtracer_protobuf protobuf::libprotobuf xrt_coreutil)
add_dependencies(xrt_trace xbtracer_common xbtracer_protobuf xrt_coreutil)

################################################################
# Create xbtracer tracer executable
################################################################
file(GLOB XBTRACER_CAPTURE_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/capture/*.cpp"
)

add_executable(xrt-tracer ${XBTRACER_CAPTURE_SRC_FILES})
target_link_libraries(xrt-tracer PRIVATE xbtracer_common)
target_include_directories(xrt-tracer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
 )
add_dependencies(xrt-tracer xbtracer_common)

################################################################
# Create xbtracer replay executable
################################################################
file(GLOB XBREPLAY_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/replay/*.cpp"
)
add_executable(xrt-replay ${XBREPLAY_SRC_FILES})
target_include_directories(xrt-replay PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR} 
  ${CMAKE_CURRENT_BINARY_DIR}/src
 )
target_link_libraries(xrt-replay PRIVATE xbtracer_common xbtracer_protobuf protobuf::libprotobuf xrt_coreutil)
if (NOT WIN32)
  target_link_libraries(xrt-replay PRIVATE pthread)
endif (NOT WIN32)
add_dependencies(xrt-replay xbtracer_common xbtracer_protobuf xrt_coreutil)

# TODO: when buiding with yocto for APU in CI, the status return from
# message to jason convertion function provided from protobuf built
# from yocto doesn't match the one in the absl library, which results
# in build failure. After fixing this issue in yocto APU build, we can
# always print message as JSON.  for now, we by default disable it, as
# it is not the key feature in the tracer/replay prototype, and replay
# only tries to dump JSON message when it fails to replay a function.
if (XRT_XBTRACER_ENABLE_JSON)
  target_compile_options(xrt-replay PRIVATE XBTRACER_PROTOBUF_HAS_JASON)

  add_executable(xbtracer_dump
    src/misc/xbtracer_dump.cpp
  )
  target_link_libraries(xbtracer_dump PRIVATE xbtracer_common xbtracer_protobuf protobuf::libprotobuf)
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
