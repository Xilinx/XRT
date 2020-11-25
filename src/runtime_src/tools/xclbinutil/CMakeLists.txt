# -----------------------------------------------------------------------------
# Required libraries

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)

# Signing xclbin images are currently only support on Linux
if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
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
  "FormattedOutput.cxx"
  "ParameterSectionData.cxx"
  "Section.cxx"     # Note: Due to linking dependency issue, this entry needs to be before the other sections
  "Section*.cxx"
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

target_link_libraries(${XCLBINUTIL_NAME} PRIVATE ${Boost_LIBRARIES} )

install (TARGETS ${XCLBINUTIL_NAME} RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})

# ==-- x c l b i n t e s t --==================================================

if (WIN32)
  set(GTEST_ROOT "C:/Xilinx/XRT/ext")
endif()

find_package(GTest)

if (GTEST_FOUND)
  set(UNIT_TEST_NAME "xclbintest")

  # Copy the unit test to the build directory
  message (STATUS "Copying test data")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/unittests/test_data/" DESTINATION "unittests/test_data")

  enable_testing()
  message (STATUS "GTest include dirs: '${GTEST_INCLUDE_DIRS}'")
  include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

  file(GLOB XCLBINTEST_FILES
    "unittests/*.cxx"
  )

  set(XCLBINTEST_SRCS ${XCLBINTEST_FILES} ${XCLBINUTIL_FILES_SRCS})
  add_executable(${UNIT_TEST_NAME} ${XCLBINTEST_SRCS})

  if(WIN32)
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE Boost::filesystem Boost::program_options Boost::system )
    target_link_libraries(${UNIT_TEST_NAME} PRIVATE ${GTEST_BOTH_LIBRARIES})
  else()
    target_link_libraries(xclbintest PRIVATE ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread crypto)
  endif()

  message(STATUS "Configuring CMake to run unit test after building")
  add_test(NAME ${UNIT_TEST_NAME}
           COMMAND ${UNIT_TEST_NAME}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  # Execute unit tests after the xclbintest is created.
  # Note: This command works GREAT if the tests pass.  If they fail xclbintest is deleted :(
  if (0)
    add_custom_command(
         TARGET ${UNIT_TEST_NAME}
         COMMENT "Run xclbinutil unit-tests"
         POST_BUILD
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE} -R ${UNIT_TEST_NAME} --output-on-failures
      )
  endif()

  # Execute unit tests after the xclbintest is created.
  # Note: This command works GREAT if the tests pass.  If they fail xclbintest is deleted,
  #       hence this code is commented out.
  #  add_custom_command(
  #     TARGET ${UNIT_TEST_XCLBINUTIL}
  #     COMMENT "Run xclbinutil unit-tests"
  #     POST_BUILD
  #     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  #     COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE} -R ${UNIT_TEST_XCLBINUTIL} --output-on-failures
  #  )

else()
  message (STATUS "GTest was not found, skipping generation of test executables")
endif()
# -----------------------------------------------------------------------------
