# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.

message("-- Looking for HIP include files...")
if (NOT WIN32)
  # Determine library architecture for Debian/Ubuntu multiarch systems
  # (x86_64-linux-gnu, aarch64-linux-gnu)
  # CMAKE_LIBRARY_ARCHITECTURE is automatically set by CMake on Debian/Ubuntu
  # For other distros or incomplete toolchains, provide a reasonable fallback
  if (NOT CMAKE_LIBRARY_ARCHITECTURE)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
      set(CMAKE_LIBRARY_ARCHITECTURE "aarch64-linux-gnu")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
      set(CMAKE_LIBRARY_ARCHITECTURE "arm-linux-gnueabihf")
    else()
      set(CMAKE_LIBRARY_ARCHITECTURE "x86_64-linux-gnu")
    endif()
  endif()

  # When cross-compiling, prepend CMAKE_SYSROOT to absolute paths
  # Otherwise use paths directly on the build machine
  if (CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
    # Search paths for HIP cmake files:
    # - Debian/Ubuntu use multiarch: /usr/lib/<arch>/cmake/
    # - RHEL/CentOS/Fedora use: /usr/lib64/cmake/ or /usr/lib/cmake/
    # - ROCm typically installs to: /opt/rocm/lib/cmake/
    list(APPEND CMAKE_MODULE_PATH
      "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/usr/lib64/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib64/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/usr/lib/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/opt/rocm/lib/cmake/hip"
      "${CMAKE_SYSROOT}/opt/rocm/lib/cmake/amd_comgr"
    )
    # hip-config itself requires these other directories to find its dependencies
    list(APPEND CMAKE_PREFIX_PATH
      "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hsa-runtime64"
      "${CMAKE_SYSROOT}/usr/lib64/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib64/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/usr/lib64/cmake/hsa-runtime64"
      "${CMAKE_SYSROOT}/usr/lib/cmake/hip"
      "${CMAKE_SYSROOT}/usr/lib/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/usr/lib/cmake/hsa-runtime64"
      "${CMAKE_SYSROOT}/opt/rocm/lib/cmake/hip"
      "${CMAKE_SYSROOT}/opt/rocm/lib/cmake/amd_comgr"
      "${CMAKE_SYSROOT}/opt/rocm/lib/cmake/hsa-runtime64"
    )
  else()
    # Search paths for HIP cmake files:
    # - Debian/Ubuntu use multiarch: /usr/lib/<arch>/cmake/
    # - RHEL/CentOS/Fedora use: /usr/lib64/cmake/ or /usr/lib/cmake/
    # - ROCm typically installs to: /opt/rocm/lib/cmake/
    list(APPEND CMAKE_MODULE_PATH
      "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hip"
      "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/amd_comgr"
      "/usr/lib64/cmake/hip"
      "/usr/lib64/cmake/amd_comgr"
      "/usr/lib/cmake/hip"
      "/usr/lib/cmake/amd_comgr"
      "/opt/rocm/lib/cmake/hip"
      "/opt/rocm/lib/cmake/amd_comgr"
    )
    # hip-config itself requires these other directories to find its dependencies
    list(APPEND CMAKE_PREFIX_PATH
      "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hip"
      "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/amd_comgr"
      "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/hsa-runtime64"
      "/usr/lib64/cmake/hip"
      "/usr/lib64/cmake/amd_comgr"
      "/usr/lib64/cmake/hsa-runtime64"
      "/usr/lib/cmake/hip"
      "/usr/lib/cmake/amd_comgr"
      "/usr/lib/cmake/hsa-runtime64"
      "/opt/rocm/lib/cmake/hip"
      "/opt/rocm/lib/cmake/amd_comgr"
      "/opt/rocm/lib/cmake/hsa-runtime64"
    )
  endif()
else()
  set(HIP_PLATFORM "amd")
  # HIP SDK installs hip files to C:/Program Files/AMD/ROCm in windows
  # Latest version installed (6.1, 5.7 or whatever available) will be picked
  # Users can set HIP_DIR to location of HIP installation or default path is used
  list(APPEND CMAKE_MODULE_PATH
    $ENV{HIP_DIR}
    "C:/Program Files/AMD/ROCm/6.2/lib/cmake/hip"
    "C:/Program Files/AMD/ROCm/6.1/lib/cmake/hip"
    "C:/Program Files/AMD/ROCm/5.7/lib/cmake/hip"
  )
  # hip-config itself requires these other directories to find its dependencies
  list(APPEND CMAKE_PREFIX_PATH
    $ENV{HIP_DIR}
    "C:/Program Files/AMD/ROCm/6.2"
    "C:/Program Files/AMD/ROCm/6.1"
    "C:/Program Files/AMD/ROCm/5.7"
  )
endif()

if (XRT_YOCTO)
  #In Yocto flow, HIP headers and libs are found at standard sysroot path; no need to include hip-config
  message("-- Using HIP from sysroot")
else()
  include(hip-config)
  message("-- Found at ${HIP_INCLUDE_DIR}")
endif()
