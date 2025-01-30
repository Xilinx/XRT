# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2024, Advanced Micro Devices, Inc. All rights Reserved.
#
# Signing xclbin images are currently only support on Linux

cmake_minimum_required(VERSION 3.5.0)
project(XCLBINUTIL)


# This cmake project depends on some other cmake util files
# which can be found in folder ${XRT_SOURCE_DIR}/CMake.
if ("${XRT_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR, "XRT_SOURCE_DIR is not set, 
          if building xclbinutil only please set it to point <XRT>/src folder")
endif()

# --- Helper CMake files includes ---
include (${XRT_SOURCE_DIR}/CMake/settings.cmake)
include (${XRT_SOURCE_DIR}/CMake/utilities.cmake)
include (${XRT_SOURCE_DIR}/CMake/boostUtil.cmake)

# cmake 3.19 and after emits warning about this policy not being set
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
  cmake_policy(SET CMP0110 NEW)
endif(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)

if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  if (${XRT_NATIVE_BUILD} STREQUAL "yes")
    # Cannot use find_package(PythonInterp REQUIRED) as it pollutes the
    # global space and later causes failure with find_package(pybind11 ...)
    # in src/python/pybind11/CMakeLists.txt
    find_program(PYTHON_EXECUTABLE python3)
  endif()
endif()

# Windows support coming soon
if(NOT WIN32)
  # On Ubuntu 16.04 CMake support is limited
  if ((${LINUX_FLAVOR} STREQUAL "ubuntu") AND ("x${LINUX_VERSION}x" STREQUAL "x16.04x"))
    set(RapidJSON_VERSION_MAJOR 0)
  else()
    find_package(RapidJSON REQUIRED)
  endif()
endif()

# Need pthreads for the boost processes
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# --- Git ---
find_package(Git)

IF(GIT_FOUND)
  MESSAGE(STATUS "Looking for GIT - found at ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

# --- XRT Variables ---
include (${XRT_SOURCE_DIR}/CMake/xrtVariables.cmake)
include (${XRT_SOURCE_DIR}/CMake/version.cmake)

include_directories(
  ${XRT_SOURCE_DIR}/runtime_src/core/include
  ${PROJECT_BINARY_DIR}/gen
  )

if (${XRT_NATIVE_BUILD} STREQUAL "yes") 
  add_subdirectory(
    ${XRT_SOURCE_DIR}/runtime_src/tools/scripts 
    ${PROJECT_BINARY_DIR}/runtime_src/tools/scripts
    )
  enable_testing()
endif()

# ==-- x c l b i n u t i l --==================================================

# aie-pdi-transform only supports linux build currently
if(NOT WIN32)
  add_subdirectory(aie-pdi-transform)
endif()

set(XCLBINUTIL_NAME "xclbinutil")

set(XRT_LOADER_SCRIPTS ${XCLBINUTIL_NAME})

if(WIN32)
  # Add the command shell wrapper in addition to the bash shell wrapper.
  # Note: The bash shell wrapper is smart and will call this wrapper
  list(APPEND XRT_LOADER_SCRIPTS ${XCLBINUTIL_NAME}.bat)
endif()

file(GLOB XCLBINUTIL_FILES
  "DTC*.cxx"
  "FDT*.cxx"
  "CBOR.cxx"
  "RapidJsonUtilities.cxx"
  "KernelUtilities.cxx"
  "ElfUtilities.cxx"
  "FormattedOutput.cxx"
  "ParameterSectionData.cxx"
  "Section.cxx"     # Note: Due to linking dependency issue, this entry needs to be before the other sections
  "Section*.cxx"
  "Resources*.cxx"
  "XclBinClass.cxx"
  "XclBinSignature.cxx"
  "XclBinUtilities.cxx"
  "XclBinUtilMain.cxx"
)
set(XCLBINUTIL_FILES_SRCS ${XCLBINUTIL_FILES})

file(GLOB XCLBINUTIL_MAIN_FILE
  "xclbinutil.cxx"
)
set(XCLBINUTIL_SRCS ${XCLBINUTIL_MAIN_FILE} ${XCLBINUTIL_FILES_SRCS})

add_executable(${XCLBINUTIL_NAME} ${XCLBINUTIL_SRCS})

# Signing xclbin images currently is not support on windows
if(NOT WIN32)
  target_link_libraries(${XCLBINUTIL_NAME} PRIVATE crypto)
endif()

# Add compile definitions
if(NOT WIN32)
   if(${RapidJSON_VERSION_MAJOR} GREATER 0)
     target_compile_definitions(${XCLBINUTIL_NAME} PRIVATE ENABLE_JSON_SCHEMA_VALIDATION)
   endif()
endif()

target_link_libraries(${XCLBINUTIL_NAME} PRIVATE ${Boost_LIBRARIES} Threads::Threads)

# link the aie-pdi-transform static library
if(NOT WIN32)
   target_link_libraries(${XCLBINUTIL_NAME} PRIVATE transformcdo)
endif()

# Link required windows specific libraries with MinGW
if(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  target_link_libraries(${XCLBINUTIL_NAME} PRIVATE wsock32 ws2_32)
endif()

install (TARGETS ${XCLBINUTIL_NAME}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install (PROGRAMS ${XRT_LOADER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})

# ==-- x c l b i n t e s t --==================================================
SET(TEST_SUITE_NAME "xclbinutil")

include (${XRT_SOURCE_DIR}/CMake/unitTestSupport.cmake)

# OpenSSL encryption is only supported on linux
if(NOT WIN32)
  # -- Test Signing of the xclbin image using a CER formatted certificate
  xrt_add_test("signing-xclbin_CER" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/signXclbinCER.py")

  # -- Test Signing of the xclbin image using a DER formatted certificate
  xrt_add_test("signing-xclbin_DER" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/signXclbinDER.py")
 
  # The xclbin image must contain atleast one section and platformVBNV information to meet
  # the expectations of Linux file command utility
  # -- Test --file-check option on valid and invalid xclbin images for Linux file command 
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/FileCheck")
  xrt_add_test("file-check" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/FileCheck/FileCheck.py ${TEST_OPTIONS}")
  
  # -- Test SmartNic
  # Test: SmartNic Syntax
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic")
  xrt_add_test("smartnic-syntax" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic/SectionSmartNicSyntax.py ${TEST_OPTIONS}")

  # Test: SmartNic Schema
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic")
  xrt_add_test("smartnic-format" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic/SectionSmartNicFormat.py ${TEST_OPTIONS}")

  # -- SoftKernel
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SoftKernel")
  xrt_add_test("soft-kernel" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SoftKernel/SectionSoftKernel.py ${TEST_OPTIONS}")

  # -- Partion Metadata
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/PartitionMetadata")
  xrt_add_test("partition_metadata" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/PartitionMetadata/SectionPartitionMetadata.py ${TEST_OPTIONS}")

  # -- Fixed Kernels
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/FixedKernel")
  xrt_add_test("fixed-kernel" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/FixedKernel/FixedKernel.py ${TEST_OPTIONS}")

  # -- PS Kernels
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/PSKernel")
  xrt_add_test("ps-kernel" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/PSKernel/PSKernel.py ${TEST_OPTIONS}")

  # -- Binary Images
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/BinaryImages")
  xrt_add_test("binary-images" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/BinaryImages/BinaryImages.py ${TEST_OPTIONS}")

  # -- Single Subsection
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SingleSubsection")
  xrt_add_test("single-subsection" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SingleSubsection/SingleSubsection.py ${TEST_OPTIONS}")

  # -- AIE Partition
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/AIEPartition")
  xrt_add_test("aie-partition" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/AIEPartition/AIEPartition.py ${TEST_OPTIONS}")

  # -- BMC Section
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/BMC")
  xrt_add_test("bmc" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/BMC/BMCSection.py ${TEST_OPTIONS}")

  # -- MCS Section
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/MCS")
  xrt_add_test("mcs" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/MCS/MCSSection.py ${TEST_OPTIONS}")

  # -- IP_MEDATADA Section
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/IPMetadata")
  xrt_add_test("ip-metadata" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/IPMetadata/IPMetadata.py ${TEST_OPTIONS}")

  # -- AIE RESOURCES BIN Section
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/AieResourcesBin")
  xrt_add_test("AieResourcesBin" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/AieResourcesBin/SectionAieResourcesBin.py ${TEST_OPTIONS}")

  # -- AIE TRACE METADATA Section
  set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/AieTraceMetadata")
  xrt_add_test("AieTraceMetadata" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/AieTraceMetadata/SectionAieTraceMetadata.py ${TEST_OPTIONS}")


endif()


if (WIN32)
  set(GTEST_ROOT "C:/Xilinx/XRT/ext")
endif()

find_package(GTest)

if (GTEST_FOUND)
  set(UNIT_TEST_NAME "xclbintest")

  include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

  # Build everything in the unittest directory
  file(GLOB XCLBINTEST_FILES
    "unittests/*.cxx"
  )

  set(XCLBINTEST_SRCS ${XCLBINTEST_FILES} ${XCLBINUTIL_FILES_SRCS})
  add_executable(${UNIT_TEST_NAME} ${XCLBINTEST_SRCS})

  if(WIN32)
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE Boost::program_options Boost::system )
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE ${GTEST_BOTH_LIBRARIES})
  else()
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread crypto)

    if(NOT (${RapidJSON_VERSION_MAJOR} EQUAL 0))
      target_compile_definitions(${UNIT_TEST_NAME} PRIVATE ENABLE_JSON_SCHEMA_VALIDATION)
    endif()
  endif()

  # link the aie-pdi-transform static library
  if(NOT WIN32)
     target_link_libraries(${UNIT_TEST_NAME} PRIVATE transformcdo)
  endif()

  # Add the test
  set(TEST_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/${UNIT_TEST_NAME}")
  set(TEST_OPTIONS "--quiet --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/test_data")
  xrt_add_test(${UNIT_TEST_NAME} "${TEST_EXECUTABLE}" "${TEST_OPTIONS}")

else()
  message (STATUS "GTest was not found, skipping generation of test executables")
endif()
# -----------------------------------------------------------------------------
