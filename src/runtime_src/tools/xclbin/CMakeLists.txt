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

file(GLOB XCLBINUTIL_LIBRARY_FILES
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

set(XCLBINUTIL_LIBRARY_SRC ${XCLBINUTIL_LIBRARY_FILES})
add_library(lib_xclbinutil STATIC ${XCLBINUTIL_LIBRARY_SRC})
set_target_properties(lib_xclbinutil PROPERTIES OUTPUT_NAME "_xclbinutil")
target_link_libraries(lib_xclbinutil -static ${Boost_LIBRARIES})

#add_library(lib_shared_xclbinutil SHARED ${XCLBINUTIL_LIBRARY_SRC})
#set_target_properties(lib_shared_xclbinutil PROPERTIES OUTPUT_NAME "_xclbinutil")
#set_property(TARGET lib_shared_xclbinutil PROPERTY POSITION_INDEPENDENT_CODE ON)
#target_link_libraries(lib_shared_xclbinutil ${Boost_LIBRARIES})

file(GLOB XCLBINUTIL_FILES
  "xclbinutil.cxx"
)

set(XCLBINUTIL_SRC ${XCLBINUTIL_FILES} ${XCLBINUTIL_LIBRARY_FILES})

add_executable(xclbinutil ${XCLBINUTIL_SRC})
target_link_libraries(xclbinutil -static ${Boost_LIBRARIES} )

# -----------------------------------------------------------------------------

install (TARGETS xclbincat xclbinsplit xclbinutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

## -----------------------------------------------------------------------------
#
#find_package(GTest)
#
#if (GTEST_FOUND)
#  message (STATUS "Copying test data")
#  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/unittests/test_data/" DESTINATION "unittests/test_data")
#
#  enable_testing()
#  message (STATUS "GTest include dirs: '${GTEST_INCLUDE_DIRS}'")
#  include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
#
#  file(GLOB XCLBINTEST_FILES
#    "unittests/*.cxx"
#  )
#
#  set(XCLBINTEST_SRC ${XCLBINTEST_FILES})
#  add_executable(xclbintest ${XCLBINTEST_SRC})
#
#  message (STATUS "GTest libraries: '${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread lib_shared_xclbinutil'")
#  target_link_libraries(xclbintest ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread lib_shared_xclbinutil)
#else()
#  message (STATUS "GTest was not found, skipping generation of test executables")
#endif()
