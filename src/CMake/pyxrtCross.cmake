# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#
# This cmake file builds pyxrt when XRT is cross compiled for edge.
# find_package(Python3) cannot be used: Yocto/CMake locate the host
# interpreter, whose version does not match the Python headers in the
# sysroot.  The extension is compiled against those headers; symbols
# are resolved when python loads the module on the target.
#
# Optional overrides:
# XRT_PYTHON_INCLUDE_DIR
# XRT_PYBIND11_INCLUDE_DIR

set (xrt_py_roots ${CMAKE_SYSROOT} ${CMAKE_FIND_ROOT_PATH} ${sysroot})
list (REMOVE_ITEM xrt_py_roots "")
list (REMOVE_DUPLICATES xrt_py_roots)

# --- Python headers ---
# Prefer the highest python3* directory that contains Python.h
if (NOT XRT_PYTHON_INCLUDE_DIR)
  set (xrt_py_version "0.0")
  foreach (root ${xrt_py_roots})
    file (GLOB candidates "${root}/usr/include/python3*" "${root}/include/python3*")
    foreach (candidate ${candidates})
      if (EXISTS "${candidate}/Python.h"
          AND candidate MATCHES "python([0-9]+)\\.([0-9]+)")
        set (version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
        if (version VERSION_GREATER xrt_py_version)
          set (xrt_py_version ${version})
          set (XRT_PYTHON_INCLUDE_DIR ${candidate})
        endif()
      endif()
    endforeach()
  endforeach()
endif()

if (XRT_PYTHON_INCLUDE_DIR AND XRT_PYTHON_INCLUDE_DIR MATCHES "python([0-9]+)\\.([0-9]+)")
  set (xrt_py_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
endif()

# --- pybind11 headers ---
if (NOT XRT_PYBIND11_INCLUDE_DIR)
  foreach (root ${xrt_py_roots})
    file (GLOB candidates
      "${root}/usr/include"
      "${root}/include"
      "${root}/usr/lib/python3*/site-packages/pybind11/include"
      "${root}/usr/lib/python3*/dist-packages/pybind11/include")
    foreach (candidate ${candidates})
      if (NOT XRT_PYBIND11_INCLUDE_DIR AND EXISTS "${candidate}/pybind11/pybind11.h")
        set (XRT_PYBIND11_INCLUDE_DIR ${candidate})
      endif()
    endforeach()
  endforeach()
endif()

if (NOT XRT_PYTHON_INCLUDE_DIR OR NOT XRT_PYBIND11_INCLUDE_DIR)
  message(WARNING "-- pybind11 or python3 libs not found, pybind11 support disabled")
  return()
endif()

message("-- Python include: ${XRT_PYTHON_INCLUDE_DIR} (${xrt_py_version})")
message("-- pybind11 include: ${XRT_PYBIND11_INCLUDE_DIR}")

# pybind11_add_module is not used; it depends on find_package(Python3)
add_library(pyxrt MODULE ${CMAKE_CURRENT_SOURCE_DIR}/src/pyxrt.cpp)

target_include_directories(pyxrt SYSTEM PRIVATE
  ${XRT_PYTHON_INCLUDE_DIR}
  ${XRT_PYBIND11_INCLUDE_DIR})

target_link_libraries(pyxrt PRIVATE xrt_coreutil uuid pthread)

# Hidden visibility matches pybind11_add_module.  SUFFIX .so is used
# because the target ABI tag cannot be computed without an interpreter.
set_target_properties(pyxrt PROPERTIES
  PREFIX ""
  SUFFIX ".so"
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

set (XRT_INSTALL_PYTHON_DIR ${CMAKE_INSTALL_LIBDIR}/python${xrt_py_version}/site-packages)
message("-- pyxrt install directory: ${CMAKE_INSTALL_PREFIX}/${XRT_INSTALL_PYTHON_DIR}")

install(TARGETS pyxrt
  LIBRARY DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  )

# Install type stub file for IDE autocompletion and type checking
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pyxrt.pyi
  DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  )
