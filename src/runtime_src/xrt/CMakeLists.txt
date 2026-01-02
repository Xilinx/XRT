# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
add_subdirectory(xrt++)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT WIN32)
  include_directories(${DRM_INCLUDE_DIRS})
endif()

set(XRT_XRT_DEVICE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/device")
set(XRT_XRT_UTIL_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/util")
set(XRT_XRT_CPP_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/xrt++")

file(GLOB XRT_XRT_ALL_SRC_SHARED
  "${XRT_XRT_DEVICE_DIR}/device.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal2.cpp"
  "${XRT_XRT_UTIL_DIR}/*.cpp"
  "${XRT_XRT_CPP_DIR}/*.cpp"
  )

file(GLOB XRT_XRT_ALL_SRC_STATIC
  "${XRT_XRT_DEVICE_DIR}/device.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal_static.cpp"
  "${XRT_XRT_DEVICE_DIR}/hal2.cpp"
  "${XRT_XRT_UTIL_DIR}/*.cpp"
  "${XRT_XRT_CPP_DIR}/*.cpp"
  )

add_compile_options("-DXRT_SOURCE")

if (NOT WIN32)
set_source_files_properties(${XRT_XRT_DEVICE_DIR}/hal_static.cpp PROPERTIES
  COMPILE_FLAGS -Wno-unused
  COMPILE_DEFINITIONS XRT_LIB_DIR="${CMAKE_INSTALL_LIBDIR}"
  )
set_source_files_properties(${XRT_XRT_DEVICE_DIR}/hal.cpp PROPERTIES
  COMPILE_DEFINITIONS XRT_LIB_DIR="${CMAKE_INSTALL_LIBDIR}"
  )
endif()

xrt_configure_version_file(xrt++ SHARED)
add_library(xrt++ SHARED
  xrt++-version.rc
  ${XRT_XRT_ALL_SRC_SHARED}
  )

add_library(xrt++_static STATIC
  ${XRT_XRT_ALL_SRC_STATIC}
  )

target_link_libraries(xrt++
  PRIVATE
  xrt_coreutil
  ${Boost_SYSTEM_LIBRARY}
  )

set_target_properties(xrt++ PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (NOT WIN32)
  # On linux use xrt++
  set_target_properties(xrt++_static PROPERTIES OUTPUT_NAME "xrt++")
endif()

install(TARGETS xrt++
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)

# Install static library - exclude from xrt-targets export in yocto builds
# to avoid find_package(XRT) failures when static libs are not in sysroot
if(XRT_YOCTO)
  install(TARGETS xrt++_static
    ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT})
else()
  install(TARGETS xrt++_static
    EXPORT xrt-targets
    ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT})
endif()
