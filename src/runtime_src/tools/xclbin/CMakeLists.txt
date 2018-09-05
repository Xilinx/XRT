include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/include
  )

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)

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

set(XCLBINUTIL_SRC ${XCLBINUTIL_FILES})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lboost_program_options -D_GLIBCXX_USE_CXX11_ABI=0")
add_executable(xclbinutil ${XCLBINUTIL_SRC})
target_link_libraries(xclbinutil -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

target_link_libraries(xclbinsplit -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

find_package(GTest)
if (GTEST_FOUND)
  enable_testing()
  message (STATUS "GTest include dirs: '${GTEST_INCLUDE_DIRS}'")
  include_directories(${GTEST_INCLUDE_DIRS})
  add_executable(xclbintest unittests/main.cpp unittests/test.cpp)
  message (STATUS "GTest libraries: '${GTEST_BOTH_LIBRARIES}'")
  target_link_libraries(xclbintest ${GTEST_BOTH_LIBRARIES} pthread)
else()
  message (STATUS "GTest was not found, skipping generation of test executables")
endif()

# -----------------------------------------------------------------------------

install (TARGETS xclbincat xclbinsplit xclbinutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
