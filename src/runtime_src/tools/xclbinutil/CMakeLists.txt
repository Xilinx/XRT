# Signing xclbin images are currently only support on Linux
if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  if (${XRT_NATIVE_BUILD} STREQUAL "yes")
    find_package(PythonInterp REQUIRED)
  endif()
endif()

# Windows support coming soon
if(NOT WIN32)   
  # On Ubuntu 16.04 CMake support is limited
  if ((${LINUX_FLAVOR} STREQUAL "Ubuntu") AND (${LINUX_VERSION} STREQUAL "16.04"))
    set(RapidJSON_VERSION_MAJOR 0)
  else()
    find_package(RapidJSON REQUIRED)
  endif()
endif()

# -----------------------------------------------------------------------------
# Include the generated header files (e.g., version.h)
include_directories(${CMAKE_BINARY_DIR}/gen)

# ==-- x c l b i n u t i l --==================================================

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


target_link_libraries(${XCLBINUTIL_NAME} PRIVATE ${Boost_LIBRARIES})

install (TARGETS ${XCLBINUTIL_NAME} RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})

# ==-- x c l b i n t e s t --==================================================
SET(TEST_SUITE_NAME "xclbinutil")

# OpenSSL encryption is only supported on linux
if(NOT WIN32)
  # Python is currently not installed on Edge builds
  if (${XRT_NATIVE_BUILD} STREQUAL "yes")
    # -- Test Signing of the xclbin image using a CER formatted certificate
    xrt_add_test("signing-xclbin_CER" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/signXclbinCER.py")

    # -- Test Signing of the xclbin image using a DER formatted certificate
    xrt_add_test("signing-xclbin_DER" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/signXclbinDER.py")

    # -- Test SmartNic
    # Test: SmartNic Syntax
    set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic")
    xrt_add_test("smartnic-syntax" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic/SectionSmartNicSyntax.py ${TEST_OPTIONS}")

    # Test: SmartNic Schema
    set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic")
    xrt_add_test("smartnic-format" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SmartNic/SectionSmartNicFormat.py ${TEST_OPTIONS}")

    # -- SoftKernel
    set(TEST_OPTIONS " --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/SoftKernel")
    xrt_add_test("softkernel" "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unittests/SoftKernel/SectionSoftKernel.py ${TEST_OPTIONS}")

    endif()
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
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE Boost::filesystem Boost::program_options Boost::system )
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE ${GTEST_BOTH_LIBRARIES})
  else()
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread crypto)

    if(${RapidJSON_VERSION_MAJOR} GREATER_EQUAL 1)
      target_compile_definitions(${UNIT_TEST_NAME} PRIVATE ENABLE_JSON_SCHEMA_VALIDATION)
    endif()
  endif()

  # Add the test 
  set(TEST_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/${UNIT_TEST_NAME}")
  set(TEST_OPTIONS "--quiet --resource-dir ${CMAKE_CURRENT_SOURCE_DIR}/unittests/test_data")
  xrt_add_test(${UNIT_TEST_NAME} "${TEST_EXECUTABLE}" "${TEST_OPTIONS}")

else()
  message (STATUS "GTest was not found, skipping generation of test executables")
endif()
# -----------------------------------------------------------------------------
