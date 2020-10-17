# -----------------------------------------------------------------------------

set(Boost_USE_STATIC_LIBS ON)               # Only find static libraries
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

# -----------------------------------------------------------------------------

include_directories(${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------

# Collect files outside of this directory
file(GLOB XBMGMT_V2_BASE_FILES
  "xbmgmt.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilities.cpp"
  "../common/SubCmd.cpp"
  "../common/XBHelpMenus.cpp"
  "../common/OptionOptions.cpp"
  "../common/ProgressBar.cpp"
  "../common/Report*.cpp"
)

# Collect local directory files
file(GLOB XBMGMT_V2_SUBCMD_FILES
  "SubCmdProgram.cpp"
  "SubCmdReset.cpp"
  "SubCmdExamine.cpp"
  "SubCmdAdvanced.cpp"
  "OO_Clock.cpp"
  "OO_Config.cpp"
  "OO_NIFD.cpp"
  "Report*.cpp"
  "flash/*.cpp"
)

# Merge the files into one collection
set(XBMGMT_V2_FILES_SRCS ${XBMGMT_V2_BASE_FILES} ${XBMGMT_V2_SUBCMD_FILES})
set(XBMGMT_V2_SRCS ${XBMGMT_V2_FILES_SRCS})


# Determine the name of the executable
if(WIN32)
  set(XBMGMT2_NAME "xbmgmt")     # Yes, on windows the file name will be xbmgmt
  set(XRT_LOADER_SCRIPTS "xbmgmt" "xbmgmt.bat")
else()
  set(XBMGMT2_NAME "xbmgmt2")
  set(XRT_LOADER_SCRIPTS "xbmgmt")
endif()

add_executable(${XBMGMT2_NAME} ${XBMGMT_V2_SRCS})

# Determine what functionality should be added
if(WIN32)
  target_compile_definitions(${XBMGMT2_NAME} PUBLIC ENABLE_DEPRECATED_2020_1_SUBCMDS)
else()
  target_compile_definitions(${XBMGMT2_NAME} PUBLIC ENABLE_DEPRECATED_2020_1_SUBCMDS)
endif()

# Add the supporting libraries
if(WIN32)
target_link_libraries(
    ${XBMGMT2_NAME} PRIVATE
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    Boost::filesystem
    Boost::program_options
    Boost::system
    ws2_32
    xrt_core
    xrt_coreutil
  )

else()
  target_link_libraries(
    ${XBMGMT2_NAME}
    xrt_core
    xrt_core_static
    xrt_coreutil_static
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    uuid
    dl
  )
endif()

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  install (TARGETS ${XBMGMT2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
  install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})
endif()

# -----------------------------------------------------------------------------
