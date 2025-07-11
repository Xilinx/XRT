# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Support building XRT or its components with local build of protobuf libraries.
SET (Protobuf_DEBUG)
INCLUDE(FindProtobuf)
if (DEFINED ENV{XRT_PROTOBUF_INSTALL})
  set(XRT_PROTOBUF_INSTALL $ENV{XRT_PROTOBUF_INSTALL})
  if (WIN32)
    # CMake FindProtobuf is not able to solve finding header
    # or libraries in case when we only include header and prebuilt
    # libraries in our prebuilt location but not including
    # protobuf.pc.in which the default CMake FindProtobuf is looking
    # for when MSVC is used. And thus, we set the include and the
    # libraries first before we call the FindProtobuf to look for
    # others such as Protobuf compiler in windows.
    set(Protobuf_INCLUDE_DIR "${XRT_PROTOBUF_INSTALL}/include")
    set(protobuf_libname "libprotobuf")
    find_library(Protobuf_LIBRARY_RELEASE
      NAMES ${protobuf_libname}
      NAMES_PER_DIR
      PATHS ${XRT_PROTOBUF_INSTALL}/lib)
    mark_as_advanced(Protobuf_LIBRARY_RELEASE)
    find_library(Protobuf_LIBRARY_DEBUG
      NAMES ${protobuf_libname}d ${protobuf_libname}
      NAMES_PER_DIR
      PATHS ${XRT_PROTOBUF_INSTALL}/lib)
    mark_as_advanced(Protobuf_LIBRARY_DEBUG)
    select_library_configurations(Protobuf)
  endif (WIN32)
  find_package(Protobuf
    HINTS ${XRT_PROTOBUF_INSTALL}
    REQUIRED)
else ()
  message("no XRT installed protobuf, checking system wide for protobuf.")
  find_package(Protobuf)
endif()
