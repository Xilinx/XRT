# -----------------------------------------------------------------------------

set(Boost_USE_STATIC_LIBS ON)               # Only find static libraries
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries
set(Boost_USE_STATIC_RUNTIME ON)            # This is how the boost libraries were build

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

# -----------------------------------------------------------------------------

include_directories(${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------

file(GLOB XBMGMT_V2_BASE_FILES
  "xbmgmt.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilities.cpp"
  "../common/SubCmd.cpp"
)

if(WIN32)
  set(XBMGMT2_NAME "xbmgmt")     # Yes, on windows the file name will be xbmgmt
  file(GLOB XBMGMT_V2_SUBCMD_FILES
    "SubCmdFlash.cpp"
    "SubCmdVersion.cpp"
  )
else()
  set(XBMGMT2_NAME "xbmgmt2")
  file(GLOB XBMGMT_V2_SUBCMD_FILES
    "SubCmdFlash.cpp"
    "SubCmdVersion.cpp"
  )
endif()

set(XBMGMT_V2_FILES_SRCS ${XBMGMT_V2_BASE_FILES} ${XBMGMT_V2_SUBCMD_FILES})
set(XBMGMT_V2_SRCS ${XBMGMT_V2_FILES_SRCS})

add_executable(${XBMGMT2_NAME} ${XBMGMT_V2_SRCS})


if(WIN32) 
  target_link_libraries(
    ${XBMGMT2_NAME} PRIVATE 
    Boost::filesystem 
    Boost::program_options 
    Boost::system 
    xrt_core
    xrt_core_static
    xrt_coreutil_static
  )

  # Use the static version of the run-time library 
  # Note: Needed to link with boost
  if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_options(${XBMGMT2_NAME} PRIVATE /MTd)
  else()
    target_compile_options(${XBMGMT2_NAME} PRIVATE /MT)
  endif()
else()
  target_link_libraries(
    ${XBMGMT2_NAME} 
    xrt_core_static
    xrt_coreutil_static
    boost_filesystem
    boost_system
    boost_program_options 
    uuid
    dl
  )
endif()

install (TARGETS ${XBMGMT2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# -----------------------------------------------------------------------------
