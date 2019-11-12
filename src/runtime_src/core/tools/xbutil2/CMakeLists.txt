# -----------------------------------------------------------------------------

set(Boost_USE_STATIC_LIBS ON)               # Only find static libraries
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries
set(Boost_USE_STATIC_RUNTIME ON)            # This is how the boost libraries were build

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

# -----------------------------------------------------------------------------

include_directories(${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------

file(GLOB XBUTIL_V2_FILES
  "xbutil.cpp"
  "XBUtilMain.cpp"
  "XBUtilities.cpp"
  "SubCmdQuery.cpp"
  "SubCmdClock.cpp"
  "SubCmdDmaTest.cpp"
  "SubCmdDump.cpp"
  "SubCmdM2MTest.cpp"
  "SubCmdScan.cpp"
  "SubCmdProgram.cpp"
  "SubCmdRun.cpp"
  "SubCmdFan.cpp"
  "SubCmdList.cpp"
  "SubCmdMem.cpp"
  "SubCmdDD.cpp"
  "SubCmdTop.cpp"
  "SubCmdValidate.cpp"
  "SubCmdReset.cpp"
  "SubCmdP2P.cpp"
  "SubCmdVersion.cpp"
  "XBReport.cpp"
  "XBDatabase.cpp"
)
set(XBUTIL_V2_FILES_SRCS ${XBUTIL_V2_FILES})
set(XBUTIL_V2_SRCS ${XBUTIL_V2_FILES_SRCS})

add_executable(xbutil2 ${XBUTIL_V2_SRCS})


if(WIN32) 
  target_link_libraries(
    xbutil2 PRIVATE 
    Boost::filesystem 
    Boost::program_options 
    Boost::system 
    xrt_core_static
    xrt_coreutil_static
  )

  # Use the static version of the run-time library 
  # Note: Needed to link with boost
  if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_options(xbutil2 PRIVATE /MTd)
  else()
    target_compile_options(xbutil2 PRIVATE /MT)
  endif()
else()
  target_link_libraries(
    xbutil2 
    xrt_core_static
    xrt_coreutil_static
    boost_filesystem
    boost_system
    boost_program_options 
    uuid
    dl
  )
endif()

install (TARGETS xbutil2 RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# -----------------------------------------------------------------------------

