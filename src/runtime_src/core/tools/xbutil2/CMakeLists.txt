# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#
# Collect files outside of this directory

file(GLOB XBUTIL_V2_BASE_FILES
  "xbutil.cpp"
  "../common/XBMain.cpp"
  "../common/XBUtilitiesCore.cpp"
  "../common/XBUtilities.cpp"
  "../common/JSONConfigurable.cpp"
  "../common/SubCmd.cpp"
  "../common/SubCmdJsonObjects.cpp"
  "../common/OptionOptions.cpp"
  "../common/XBHelpMenusCore.cpp"
  "../common/XBHelpMenus.cpp"
  "../common/reports/*.cpp"
  "../common/reports/platform/*.cpp"
  "../common/reports/aie/*.cpp"
  "../common/Report.cpp"
  "../common/ProgressBar.cpp"
  "../common/BusyBar.cpp"
  "../common/Process.cpp"
  "../common/Table2D.cpp"
  "../common/PsKernelUtilities.cpp"
  "../common/SubCmdJSON.cpp"
  "../common/TestRunner.cpp"
  "../common/SmiDefault.cpp"
  "../common/tests/*.cpp"
  "../common/tests/aie_pl_util/*"
  "../common/tests/ps_iops_util/*"
  "../common/tests/xrt_iops_util/*"
)

# Collect local directory files
file(GLOB XBUTIL_V2_SUBCMD_FILES
  "SubCmdExamine.cpp"
  "SubCmdProgram.cpp"
  "SubCmdReset.cpp"
  "SubCmdValidate.cpp"
  "SubCmdAdvanced.cpp"
  "SubCmdConfigure.cpp"
  "OO_Clock.cpp"
  "OO_MemRead.cpp"
  "OO_MemWrite.cpp"
  "OO_P2P.cpp"
  "OO_AieRegRead.cpp"
  "OO_AieClockFreq.cpp"
  "OO_HostMem.cpp"
  "OO_Performance.cpp"
  "OO_Preemption.cpp"
)

# Merge the files into one collection
set(XBUTIL_V2_SRCS ${XBUTIL_V2_BASE_FILES} ${XBUTIL_V2_SUBCMD_FILES})

set(XBUTIL2_NAME "xrt-smi") 
# Determine any helper scripts
if(WIN32)
  set(XRT_HELPER_SCRIPTS "xrt-smi" "xrt-smi.bat")
else()
  set(XRT_HELPER_SCRIPTS "xbutil" "xrt-smi")
endif()

add_executable(${XBUTIL2_NAME} ${XBUTIL_V2_SRCS})

# Determine what functionality should be added
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  target_compile_definitions(${XBUTIL2_NAME} PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
else()
  target_compile_definitions(${XBUTIL2_NAME} PRIVATE ENABLE_DEFAULT_ONE_DEVICE_OPTION)
endif()

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${XBUTIL2_NAME}_static ${XBUTIL_V2_SRCS})
  target_compile_definitions(${XBUTIL2_NAME}_static PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
  target_link_options(${XBUTIL2_NAME}_static PRIVATE "-static" "-L${Boost_LIBRARY_DIRS}")
  # Bypass FindBoost versions and just link explicitly with boost libraries
  # The -static link option will pick the static libraries.
  target_link_libraries(${XBUTIL2_NAME}_static
    PRIVATE
    xrt_coreutil_static
    boost_system
    boost_program_options
    -Wl,--whole-archive rt pthread -Wl,--no-whole-archive
    uuid
    dl
    )
  set_target_properties(${XBUTIL2_NAME}_static PROPERTIES INSTALL_RPATH "")
  install(TARGETS ${XBUTIL2_NAME}_static
    RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
endif()

target_link_libraries(${XBUTIL2_NAME}
  PRIVATE
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )

if (NOT WIN32)
  target_link_libraries(${XBUTIL2_NAME} PRIVATE pthread uuid dl)
else()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(${XBUTIL2_NAME} PRIVATE ws2_32)
  endif()
endif()

# Package xrt sub commands json file for embedded builds
# This file acts as sample json file and will be removed in future releases
if (${XRT_NATIVE_BUILD} STREQUAL "no")
  set(XRT_SUBCOMMANDS_JSON ../common/xrt_subcommands.json)
  install(FILES ${XRT_SUBCOMMANDS_JSON}
    DESTINATION ${XRT_INSTALL_DIR}/share COMPONENT ${XRT_BASE_COMPONENT})
endif()

# Install our built executable
install (TARGETS ${XBUTIL2_NAME}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install (PROGRAMS ${XRT_HELPER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})

# -----------------------------------------------------------------------------
