include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/include
  )

# -----------------------------------------------------------------------------

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)

# -----------------------------------------------------------------------------

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# -----------------------------------------------------------------------------


file(GLOB XCLBINCAT_FILES
  "xclbincat.cxx"
  "xclbincat1.cxx"
  "xclbindata.cxx"
  "xclbinutils.cxx"
  )

set(XCLBINCAT_SRC ${XCLBINCAT_FILES})

add_executable(xclbincat ${XCLBINCAT_SRC})
target_link_libraries(xclbincat -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

target_link_libraries(xclbincat -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

file(GLOB XCLBINSPLIT_FILES
  "xclbinsplit.cxx"
  "xclbinsplit1.cxx"
  "xclbindata.cxx"
  "xclbinutils.cxx"
)

set(XCLBINSPLIT_SRC ${XCLBINSPLIT_FILES})

add_executable(xclbinsplit ${XCLBINSPLIT_SRC})
target_link_libraries(xclbinsplit -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

file(GLOB XCLBINUTIL_FILES
  "ParameterSectionData.cxx"
  "XclBinUtilMain.cxx"
  "XclBin.cxx"
  "SectionHeader.cxx"
  "XclBinUtilities.cxx"
  "Section.cxx"
  "SectionBitstream.cxx"
  "SectionClearBitstream.cxx"
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
)
set(XCLBINUTIL_FILES_SRCS ${XCLBINUTIL_FILES})

file(GLOB XCLBINUTIL_MAIN_FILE
  "xclbinutil.cxx"
)

set(XCLBINUTIL_SRCS ${XCLBINUTIL_MAIN_FILE} ${XCLBINUTIL_FILES_SRCS} "${CMAKE_CURRENT_BINARY_DIR}/generated)/version.h")

add_executable(xclbinutil ${XCLBINUTIL_SRCS})
target_link_libraries(xclbinutil -static ${Boost_LIBRARIES})

# --- NOTE ---
# It is intential that the version.h file is regenerated each time the build
# is performed. This causes xclbintil and any sources that includes this header
# file to be rebuilt. 
# 
# Ultimately, this functionality should be moved to the top most CMakeList target
# ---
add_custom_command(
OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated)/version.h"
COMMAND ${CMAKE_COMMAND}
    -Dlocal_dir="${CMAKE_CURRENT_SOURCE_DIR}"
    -Doutput_dir="${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake"
#  COMMENT "Generating generated/version.h header file"
)

# -----------------------------------------------------------------------------

install (TARGETS xclbincat xclbinsplit xclbinutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# -----------------------------------------------------------------------------

find_package(GTest)

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
  add_executable(xclbintest ${XCLBINTEST_SRCS})

  message (STATUS "GTest libraries: '${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread'")
  target_link_libraries(xclbintest ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread )
else()
  message (STATUS "GTest was not found, skipping generation of test executables")
endif()
