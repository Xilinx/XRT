# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH

# install under c:/xrt
set (CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/xilinx")

# pdb install dir
set (CMAKE_PDB_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/symbols")

# --- Git ---
find_package(Git)

IF(GIT_FOUND)
  MESSAGE(STATUS "Looking for GIT - found at ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

# --- Boost ---
#set(Boost_DEBUG 1)

INCLUDE (FindBoost)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
  REQUIRED COMPONENTS system filesystem program_options)

# Boost_VERSION_STRING is not working properly, use our own macro
set(XRT_BOOST_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})

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

if (MSVC)
    add_compile_options(
        /Zc:__cplusplus
        /Zi           # generate pdb files even in release mode
	/sdl          # enable security checks
        /Qspectre     # compile with the Spectre mitigations switch
        /ZH:SHA_256   # enable secure source code hashing
        /guard:cf     # enable compiler control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    )
    add_link_options(
        /DEBUG      # instruct linker to create debugging info
        /guard:cf   # enable linker control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
        /CETCOMPAT  # enable Control-flow Enforcement Technology (CET) Shadow Stack mitigation
    )
endif()


INCLUDE (FindGTest)

# --- XRT Variables ---
set (XRT_INSTALL_DIR "xrt")
set (XRT_INSTALL_BIN_DIR       "${XRT_INSTALL_DIR}")
set (XRT_INSTALL_UNWRAPPED_DIR "${XRT_INSTALL_BIN_DIR}/unwrapped")
set (XRT_INSTALL_INCLUDE_DIR   "${XRT_INSTALL_DIR}/include")
set (XRT_INSTALL_LIB_DIR       "${XRT_INSTALL_DIR}/lib")
set (XRT_INSTALL_PYTHON_DIR    "${XRT_INSTALL_DIR}/python")

# --- Release: eula ---
file(GLOB XRT_EULA
  "license/*.txt"
  )
install (FILES ${XRT_SOURCE_DIR}/../LICENSE DESTINATION ${XRT_INSTALL_DIR}/license)
message("-- XRT EA eula files  ${XRT_SOURCE_DIR}/../LICENSE")

# -- CPack
include (CMake/cpackWin.cmake)

# --- Create Version header and JSON file ---
include (CMake/version.cmake)

message ("------------ xrt install dir: ${XRT_INSTALL_DIR}")
add_subdirectory(runtime_src)

# --- Find Package Support ---
include (CMake/findpackage.cmake)
