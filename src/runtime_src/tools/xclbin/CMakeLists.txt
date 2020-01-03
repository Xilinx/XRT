# -----------------------------------------------------------------------------

set(Boost_USE_STATIC_LIBS ON)               # Only find static libraries
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries
set(Boost_USE_STATIC_RUNTIME ON)            # This is how the boost libraries were build

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

if(NOT WIN32)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_library(CRYPTO_LIB libcrypto.a REQUIRED)

find_package(ZLIB REQUIRED)
find_library(Z_LIB libz.a REQUIRED)

find_library(DL_LIB libdl.a REQUIRED)

find_library(PTHREAD_LIB libpthread.a)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
endif()

# -----------------------------------------------------------------------------

include_directories(${CMAKE_BINARY_DIR}/gen)

# -----------------------------------------------------------------------------

if(NOT WIN32)    # We don't support xclbincat on windows

file(GLOB XCLBINCAT_FILES
  "xclbincat.cxx"
  "xclbincat1.cxx"
  "xclbindata.cxx"
  "xclbinutils.cxx"
  )

set(XCLBINCAT_SRC ${XCLBINCAT_FILES})

add_executable(xclbincat ${XCLBINCAT_SRC})
target_link_libraries(xclbincat -static ${Boost_LIBRARIES})
# On Ubuntu 18.04 CMake was appending '-Wl,-Bdynamic' to the command line which
# was causing the executable to reference 'linux-vdso.so.1' which would then
# produce "Command not found" when running the executable.
#   Therefore, we direct CMake to end with static, so that it doesn't do that:
set_target_properties(xclbincat PROPERTIES LINK_SEARCH_END_STATIC 1)

install (TARGETS xclbincat RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

endif()
# -----------------------------------------------------------------------------

if(NOT WIN32)    # We don't support xclbinsplit on windows

file(GLOB XCLBINSPLIT_FILES
  "xclbinsplit.cxx"
  "xclbinsplit1.cxx"
  "xclbindata.cxx"
  "xclbinutils.cxx"
)

set(XCLBINSPLIT_SRC ${XCLBINSPLIT_FILES})

add_executable(xclbinsplit ${XCLBINSPLIT_SRC})
target_link_libraries(xclbinsplit -static ${Boost_LIBRARIES})
# On Ubuntu 18.04 CMake was appending '-Wl,-Bdynamic' to the command line which
# was causing the executable to reference 'linux-vdso.so.1' which would then
# produce "Command not found" when running the executable.
#   Therefore, we direct CMake to end with static, so that it doesn't do that:
set_target_properties(xclbinsplit PROPERTIES LINK_SEARCH_END_STATIC 1)

install (TARGETS xclbinsplit RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
endif()

# -----------------------------------------------------------------------------

file(GLOB XCLBINUTIL_FILES
  "FormattedOutput.cxx"
  "ParameterSectionData.cxx"
  "XclBinUtilMain.cxx"
  "XclBinClass.cxx"
  "SectionHeader.cxx"
  "XclBinUtilities.cxx"
  "Section.cxx"
  "SectionBitstream.cxx"
  "SectionClearBitstream.cxx"
  "SectionDNACertificate.cxx"
  "SectionEmbeddedMetadata.cxx"
  "SectionManagementFW.cxx"
  "SectionDebugData.cxx"
  "SectionSchedulerFW.cxx"
  "SectionMemTopology.cxx"
  "SectionConnectivity.cxx"
  "SectionIPLayout.cxx"
  "SectionDebugIPLayout.cxx"
  "SectionDesignCheckPoint.cxx"
  "SectionClockFrequencyTopology.cxx"
  "SectionMCS.cxx"
  "SectionBMC.cxx"
  "SectionBuildMetadata.cxx"
  "SectionKeyValueMetadata.cxx"
  "SectionUserMetadata.cxx"
  "SectionPDI.cxx"
  "SectionBitstreamPartialPDI.cxx"
  "SectionPartitionMetadata.cxx"
  "SectionEmulationData.cxx"
  "SectionSystemMetadata.cxx"
  "SectionSoftKernel.cxx"
  "SectionFlash.cxx"
  "DTC.cxx"
  "DTCStringsBlock.cxx"
  "FDTNode.cxx"
  "FDTProperty.cxx"
  "XclBinSignature.cxx"
)
set(XCLBINUTIL_FILES_SRCS ${XCLBINUTIL_FILES})

file(GLOB XCLBINUTIL_MAIN_FILE
  "xclbinutil.cxx"
)
set(XCLBINUTIL_SRCS ${XCLBINUTIL_MAIN_FILE} ${XCLBINUTIL_FILES_SRCS})

add_executable(xclbinutil ${XCLBINUTIL_SRCS})

if(WIN32)
  target_link_libraries(xclbinutil PRIVATE Boost::filesystem Boost::program_options Boost::system)

  # Use the static version of the run-time library
  # Note: Needed to link with boost
  if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_options(xclbinutil PRIVATE /MTd)
  else()
    target_compile_options(xclbinutil PRIVATE /MT)
  endif()
else()

  # For Ubuntu 18.04 and later versions we must link dynamic libraries.  This is because the static
  # openssl library is linked to a dynamic library :-(
  if ((${LINUX_FLAVOR} STREQUAL Ubuntu) AND (${LINUX_VERSION} VERSION_GREATER 17.10))
    # Note: The following Ubuntu 18.04 target results in a execution Segmentation Fault
    #       due to dynamic library dependencies in the openssl static library.
    #
    #       The offending library is: libdl.so.2
    #
    #   target_link_libraries(xclbinutil -static ${Boost_LIBRARIES})
    #   target_link_libraries(xclbinutil ${CRYPTO_LIB})
    #   target_link_libraries(xclbinutil ${Z_LIB})       # OpenSSL supporting library
    #   target_link_libraries(xclbinutil ${DL_LIB})      # OpenSSL supporting library
    #   target_link_libraries(xclbinutil ${PTHREAD_LIB})      # OpenSSL supporting library
    #   target_link_libraries(xclbinutil -static-libgcc -static-libstdc++)
    #   set_target_properties(xclbinutil PROPERTIES LINK_SEARCH_END_STATIC 1)

    # Create an xclbinutil executable that is dependent on shared libraries
    # Used the linux command 'ldd' to show the shared libraries used by the executable
    target_link_libraries(xclbinutil ${Boost_LIBRARIES})
    target_link_libraries(xclbinutil OpenSSL::Crypto)
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL aarch64)
      target_link_libraries(xclbinutil pthread)
    else()
      target_link_libraries(xclbinutil -Bstatic ${PTHREAD_LIB})
    endif()
    target_link_libraries(xclbinutil ${CMAKE_DL_LIBS})   # Needs to be shared library
    target_link_libraries(xclbinutil -static-libgcc -static-libstdc++)
  else()
    target_link_libraries(xclbinutil -static ${Boost_LIBRARIES})
    target_link_libraries(xclbinutil ${CRYPTO_LIB})
    target_link_libraries(xclbinutil ${Z_LIB})            # OpenSSL supporting library
    target_link_libraries(xclbinutil ${DL_LIB})           # OpenSSL supporting library
    target_link_libraries(xclbinutil ${PTHREAD_LIB})      # OpenSSL supporting library

    # On Ubuntu 18.04 CMake was appending '-Wl,-Bdynamic' to the command line which
    # was causing the executable to reference 'linux-vdso.so.1' which would then
    # produce "Command not found" when running the executable.
    #   Therefore, we direct CMake to end with static, so that it doesn't do that:
    set_target_properties(xclbinutil PROPERTIES LINK_SEARCH_END_STATIC 1)
  endif()
endif()

install (TARGETS xclbinutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# -----------------------------------------------------------------------------

if (WIN32)
  set(GTEST_ROOT "C:/XRT/libs/gtest")
endif()

find_package(GTest)

#find_library(ws_bcrypt_LIBRARY_PATH
#  NAMES bcrypt
#  HINTS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/x64/")
#message("bcrypt = ${ws_bcrypt_LIBRARY_PATH}")
#target_link_libraries(xclbinutil PRIVATE ${ws_bcrypt_LIBRARY_PATH})


if (GTEST_FOUND)
  message (STATUS "Copying test data")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/unittests/test_data/" DESTINATION "unittests/test_data")

  enable_testing()
  message (STATUS "GTest include dirs: '${GTEST_INCLUDE_DIRS}'")
  include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

  file(GLOB XCLBINTEST_FILES
    "unittests/*.cxx"
  )

  set(XCLBINTEST_SRCS ${XCLBINTEST_FILES} ${XCLBINUTIL_FILES_SRCS})

  set(UNIT_TEST_XCLBINUTIL xclbintest)
  add_executable(${UNIT_TEST_XCLBINUTIL} ${XCLBINTEST_SRCS})

  if(WIN32) 
    target_link_libraries(${UNIT_TEST_XCLBINUTIL} PRIVATE Boost::filesystem Boost::program_options Boost::system )
    target_link_libraries(${UNIT_TEST_XCLBINUTIL} PRIVATE ${GTEST_BOTH_LIBRARIES})

    # Use the static version of the run-time library 
    # Note: Needed to link with boost
    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      target_compile_options(${UNIT_TEST_XCLBINUTIL} PRIVATE /MTd)
    else()
      target_compile_options(${UNIT_TEST_XCLBINUTIL} PRIVATE /MT)
    endif()
  else()
    message (STATUS "GTest libraries: '${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread'")
    target_link_libraries(xclbintest ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread )
    target_link_libraries(xclbintest OpenSSL::Crypto)
    target_link_libraries(xclbintest ${ZLIB_LIBRARIES})     # OpenSSL supporting library
    target_link_libraries(xclbintest ${CMAKE_DL_LIBS})      # OpenSSL supporting library
  endif()

  message(STATUS "Configuring CMake to run unit test after building")
  add_test(NAME ${UNIT_TEST_XCLBINUTIL} 
           COMMAND ${UNIT_TEST_XCLBINUTIL}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  # Execute unit tests after the xclbintest is created.
  # Note: This command works GREAT if the tests pass.  If they fail xclbintest is deleted :(
  if (0)
    add_custom_command(
         TARGET ${UNIT_TEST_XCLBINUTIL}
         COMMENT "Run xclbinutil unit-tests"
         POST_BUILD 
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE} -R ${UNIT_TEST_XCLBINUTIL} --output-on-failures
      )
  endif()

# Execute unit tests after the xclbintest is created.
# Note: This command works GREAT if the tests pass.  If they fail xclbintest is deleted :(
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
