# Collect files outside of this directory
file(GLOB XBUTIL_V2_BASE_FILES
  "xbutil.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilities.cpp"
  "../common/SubCmd.cpp"
  "../common/OptionOptions.cpp"
  "../common/XBHelpMenus.cpp"
  "../common/Report*.cpp"
  "../common/ProgressBar.cpp"
  "../common/Process.cpp"
)

# Collect local directory files
file(GLOB XBUTIL_V2_SUBCMD_FILES
  "XBReport.cpp"
  "SubCmdExamine.cpp"
  "SubCmdProgram.cpp"
  "SubCmdReset.cpp"
  "SubCmdValidate.cpp"
  "SubCmdAdvanced.cpp"
  "OO_Clock.cpp"
  "OO_MemRead.cpp"
  "OO_MemWrite.cpp"
  "OO_P2P.cpp"
)

# Merge the files into one collection
set(XBUTIL_V2_SRCS ${XBUTIL_V2_BASE_FILES} ${XBUTIL_V2_SUBCMD_FILES})

# Determine the name of the executable
if(WIN32)
  set(XBUTIL2_NAME "xbutil")     # Yes, on windows the file name will be xbutil
  set(XRT_LOADER_SCRIPTS "xbutil" "xbutil.bat")
else()
  set(XBUTIL2_NAME "xbutil2")
  set(XRT_LOADER_SCRIPTS "xbutil")
endif()

add_executable(${XBUTIL2_NAME} ${XBUTIL_V2_SRCS})

# Determine what functionality should be added
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  target_compile_definitions(${XBUTIL2_NAME} PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
endif()

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${XBUTIL2_NAME}_static ${XBUTIL_V2_SRCS})
  target_link_options(${XBUTIL2_NAME}_static PRIVATE "-static")
  # Bypass FindBoost versions and just link explicitly with boost libraries
  # The -static link option will pick the static libraries.
  target_link_libraries(${XBUTIL2_NAME}_static
    PRIVATE
    xrt_core_static
    xrt_coreutil_static
    boost_system
    boost_filesystem
    boost_program_options
    -Wl,--whole-archive rt pthread -Wl,--no-whole-archive
    uuid
    dl
    )
  set_target_properties(${XBUTIL2_NAME}_static PROPERTIES INSTALL_RPATH "")
  install(TARGETS ${XBUTIL2_NAME}_static RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
endif()

target_link_libraries(${XBUTIL2_NAME}
  PRIVATE
  xrt_core
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )

if (NOT WIN32)
  target_link_libraries(${XBUTIL2_NAME} PRIVATE pthread uuid dl)
endif()

# Install our built executable
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  install (TARGETS ${XBUTIL2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
  install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})
else()
  install (TARGETS ${XBUTIL2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR})
endif()
# -----------------------------------------------------------------------------
