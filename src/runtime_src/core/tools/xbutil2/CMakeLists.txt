# -----------------------------------------------------------------------------

set(Boost_USE_STATIC_LIBS ON)               # Only find static libraries
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

# -----------------------------------------------------------------------------

include_directories(${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------

file(GLOB XBUTIL_V2_BASE_FILES
  "xbutil.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilities.cpp"
  "XBReport.cpp"
  "XBDatabase.cpp"
  "../common/SubCmd.cpp"
)

if(WIN32)
  set(XBUTIL2_NAME "xbutil")     # Yes, on windows the file name will be xbutil
  file(GLOB XBUTIL_V2_SUBCMD_FILES
    "SubCmdProgram.cpp"
    "SubCmdClock.cpp"
    "SubCmdQuery.cpp"
    "SubCmdReset.cpp"
    "SubCmdScan.cpp"
    "SubCmdVersion.cpp"
  )
else()
  set(XBUTIL2_NAME "xbutil2")
  file(GLOB XBUTIL_V2_SUBCMD_FILES
    "SubCmdQuery.cpp"
    "SubCmdClock.cpp"
    "SubCmdDmaTest.cpp"
    "SubCmdDump.cpp"
    "SubCmdM2MTest.cpp"
    "SubCmdScan.cpp"
    "SubCmdProgram.cpp"
    "SubCmdList.cpp"
    "SubCmdMem.cpp"
    "SubCmdDD.cpp"
    "SubCmdTop.cpp"
    "SubCmdValidate.cpp"
    "SubCmdReset.cpp"
    "SubCmdP2P.cpp"
    "SubCmdVersion.cpp"
  )
endif()

set(XBUTIL_V2_FILES_SRCS ${XBUTIL_V2_BASE_FILES} ${XBUTIL_V2_SUBCMD_FILES})
set(XBUTIL_V2_SRCS ${XBUTIL_V2_FILES_SRCS})

add_executable(${XBUTIL2_NAME} ${XBUTIL_V2_SRCS})


if(WIN32)
  target_link_libraries(
    ${XBUTIL2_NAME} PRIVATE
    Boost::filesystem
    Boost::program_options
    Boost::system
    xrt_core
    xrt_core_static
    xrt_coreutil_static
  )

else()
  target_link_libraries(
    ${XBUTIL2_NAME}
    xrt_core_static
    xrt_coreutil_static
    boost_filesystem
    boost_system
    boost_program_options
    uuid
    dl
  )
endif()

install (TARGETS ${XBUTIL2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# -----------------------------------------------------------------------------
