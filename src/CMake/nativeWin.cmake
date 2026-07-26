# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#
# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH

# pdb install dir
set (CMAKE_PDB_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/symbols")

include(CMake/components.cmake)

# Boost Libraries
set(ENV{XRT_BOOST_INSTALL} "${BOOST_ROOT}")
include (CMake/boostUtil.cmake)

include_directories(${Boost_INCLUDE_DIRS})
add_compile_definitions("BOOST_LOCALE_HIDE_AUTO_PTR")
add_compile_definitions("BOOST_BIND_GLOBAL_PLACEHOLDERS")

# warning C4996: 'std::allocator<void>': warning STL4009:
# std::allocator<void> is deprecated in C++17. You can define
# _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING or
# _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to acknowledge that you have
# received this warning.
#
# Per https://developercommunity.visualstudio.com/t/boost-asio-reports-stdallocator-is-deprecated-in-c/500588
# the warning is bogus.  Remove defintion when fixed
add_compile_definitions("_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING")

option(ENABLE_ASAN "Build with AddressSanitizer (/fsanitize=address). Requires MSVC v145 (VS 2026) for ARM64." OFF)

if (MSVC)
  if (ENABLE_ASAN)
    # Always use /MD (not /MDd): ASAN has its own heap tracking and does not
    # integrate with the debug CRT. Avoids CMake 4.x try_compile rejection of
    # MultiThreadedDLLDebug for the ARM64 cross-compiler.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()

  add_compile_options(
    # Explicit /MT only on the non-ASAN path. On the ASAN path CRT selection is
    # driven by CMAKE_MSVC_RUNTIME_LIBRARY / the per-target MSVC_RUNTIME_LIBRARY
    # property: an explicit /MD here would be mapped into <RuntimeLibrary> by the
    # VS generator and override the per-target /MT that xclbinutil/xrt-smi need.
    $<$<NOT:$<BOOL:${ENABLE_ASAN}>>:/MT$<$<CONFIG:Debug>:d>>
    /Zc:__cplusplus
    /Zi           # generate pdb files even in release mode
    /sdl          # enable security checks
    /Gs           # control stack checking calls
    /Qspectre     # compile with the Spectre mitigations switch
    /ZH:SHA_256   # enable secure source code hashing
    /guard:cf     # enable compiler control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    /GF           # eliminate duplicate strings
    $<$<NOT:$<CONFIG:Debug>>:/guard:cast> # enable cast guard to prevent type confusion
    $<$<NOT:$<CONFIG:Debug>>:/d2CastGuardFailureMode:fastfail> # fastfail mode for cast guard
    $<$<NOT:$<CONFIG:Debug>>:/GL>  # enable whole program optimization
    )
  if (NOT ENABLE_ASAN)
    # Hybrid CRT is skipped under ASAN. On x64 the static ASan malloc thunk
    # (asan_malloc_win_thunk) defines malloc/free; forcing the dynamic ucrt.lib
    # import re-defines them -> LNK2005/LNK1169 in the /MT tool targets
    # (xclbinutil, xrt-smi). Keeping the standard static ucrt default for /MT lets
    # ASan intercept the allocator cleanly. No-op for /MD targets and ARM64.
    add_link_options(
      /NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib  # Hybrid CRT
      /DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib       # Hybrid CRT
      )
  endif()
  add_link_options(
    $<$<CONFIG:Debug>:/INCREMENTAL>            # enable incremental linking for debug builds
    $<$<CONFIG:Debug>:/LTCG:OFF>               # disable link time code generation for debug builds
    $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>  # disable incremental linking for release builds
    $<$<NOT:$<CONFIG:Debug>>:/LTCG>            # enable link time code generation for release builds
    $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>         # enable COMDAT folding
    $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>         # eliminates functions and data that are never referenced
    /DEBUG           # instruct linker to create debugging info
    /guard:cf        # enable linker control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    /DYNAMICBASE     # enable ASLR
    /HIGHENTROPYVA   # enable 64-bit ASLR
    /LARGEADDRESSAWARE # enable large address awareness
    /experimental:deterministic # deterministic build
    )
  if (NOT ${CMAKE_CXX_COMPILER} MATCHES "(arm64|ARM64)")
    add_link_options(
      /CETCOMPAT  # enable Control-flow Enforcement Technology (CET) Shadow Stack mitigation
      )
  endif()
  if (ENABLE_ASAN)
    # Derive ASAN lib dir from compiler path:
    # .../MSVC/<ver>/bin/Hostx64/<arch>/cl.exe -> .../MSVC/<ver>/lib/<arch>/
    get_filename_component(_asan_lib_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_asan_lib_dir "${_asan_lib_dir}" DIRECTORY)
    get_filename_component(_asan_lib_dir "${_asan_lib_dir}" DIRECTORY)
    get_filename_component(_asan_lib_dir "${_asan_lib_dir}" DIRECTORY)
    if (${CMAKE_CXX_COMPILER} MATCHES "(arm64|ARM64)")
      set(_asan_lib_dir "${_asan_lib_dir}/lib/arm64")
    else()
      set(_asan_lib_dir "${_asan_lib_dir}/lib/x64")
    endif()
    link_directories("${_asan_lib_dir}")
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
  endif()
endif()


include(FindGTest)

# --- XRT Variables ---
include(CMake/xrtVariables.cmake)

# --- Release: eula ---
file(GLOB XRT_EULA
  "license/*.txt"
  )
install (FILES ${XRT_SOURCE_DIR}/../LICENSE DESTINATION ${XRT_INSTALL_DIR}/license)
message("-- XRT EA eula files  ${XRT_SOURCE_DIR}/../LICENSE")

# --- Create Version header and JSON file ---
include(CMake/version.cmake)

message("------------ xrt install dir: ${XRT_INSTALL_DIR}")
add_subdirectory(runtime_src)

# --- Find Package Support ---
include(CMake/findpackage.cmake)

# --- Python bindings ---
if (NOT ${CMAKE_CXX_COMPILER} MATCHES "(arm64|ARM64)")
  xrt_add_subdirectory(python)
endif()

# -- CPack windows SDK if base component
if (${XRT_BASE_DEV_COMPONENT} STREQUAL "base_dev")
  include(CMake/cpack-windows-sdk.cmake)
else()
  # Legacy
  include(CMake/cpackWin.cmake)
endif()

