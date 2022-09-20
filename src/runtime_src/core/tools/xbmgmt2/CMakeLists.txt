# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# Collect files outside of this directory
file(GLOB XBMGMT_V2_BASE_FILES
  "xbmgmt.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilitiesCore.cpp"
  "../common/XBUtilities.cpp"
  "../common/SubCmd.cpp"
  "../common/XBHelpMenusCore.cpp"
  "../common/XBHelpMenus.cpp"
  "../common/OptionOptions.cpp"
  "../common/ProgressBar.cpp"
  "../common/BusyBar.cpp"
  "../common/Process.cpp"
  "../common/Report*.cpp"
  "../common/PsKernelUtilities.cpp"
  "../common/Table2D.cpp"
)

# Collect local directory files
file(GLOB XBMGMT_V2_SUBCMD_FILES
  "SubCmdProgram.cpp"
  "SubCmdReset.cpp"
  "SubCmdExamine.cpp"
  "SubCmdAdvanced.cpp"
  "SubCmdDump.cpp"
  "SubCmdConfigure.cpp"
  "OO_Hotplug.cpp"
  "OO_UpdateBase.cpp"
  "OO_UpdateShell.cpp"
  "Report*.cpp"
  "flash/*.cpp"
)

# Merge the files into one collection
set(XBMGMT_V2_SRCS ${XBMGMT_V2_BASE_FILES} ${XBMGMT_V2_SUBCMD_FILES})

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
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  target_compile_definitions(${XBMGMT2_NAME} PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
else()
  target_compile_definitions(${XBUTIL2_NAME} PRIVATE ENABLE_DEFAULT_ONE_DEVICE_OPTION)
endif()

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${XBMGMT2_NAME}_static ${XBMGMT_V2_SRCS})
  target_link_options(${XBMGMT2_NAME}_static PRIVATE "-static" "-L${Boost_LIBRARY_DIRS}")
  target_compile_definitions(${XBMGMT2_NAME}_static PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
  # Bypass FindBoost versions and just link explicitly with boost libraries
  # The -static link option will pick the static libraries. 
  target_link_libraries(${XBMGMT2_NAME}_static
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
  set_target_properties(${XBMGMT2_NAME}_static PROPERTIES INSTALL_RPATH "")
  install(TARGETS ${XBMGMT2_NAME}_static RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
endif()

target_link_libraries(${XBMGMT2_NAME}
  PRIVATE
  xrt_core
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )
  
# Add the supporting libraries
if(WIN32)
  target_link_libraries(${XBMGMT2_NAME} PRIVATE ws2_32)
else()
  target_link_libraries(${XBMGMT2_NAME} PRIVATE pthread uuid dl)
endif()

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  install (TARGETS ${XBMGMT2_NAME} RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
  install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})
endif()
