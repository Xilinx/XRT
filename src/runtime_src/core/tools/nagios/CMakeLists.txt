# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#

# This plugin should be compiled only for Linux distributions
file(GLOB NAGIOS_BASE_FILES
  "NagiosPlugin.cpp"
  "../common/XBUtilitiesCore.cpp"
  "../common/XBUtilities.cpp"
  "../common/XBHelpMenusCore.cpp"
  "../common/XBHelpMenus.cpp"
  "../common/Report*.cpp"
  "../common/Table2D.cpp"
)

set(XRT_NAGIOS_INSTALL "${XRT_INSTALL_DIR}/etc/nagios-plugins")
set(NAGIOS_SRCS ${NAGIOS_BASE_FILES})

# Determine the name of the executable
set(NAGIOS_PLUGIN_NAME "xrt_nagios_plugin")
set(NAGIOS_LOADER_SCRIPTS "nagios_plugin.sh")

add_executable(${NAGIOS_PLUGIN_NAME} ${NAGIOS_SRCS})

# Determine what functionality should be added
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  target_compile_definitions(${NAGIOS_PLUGIN_NAME} PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
else()
  target_compile_definitions(${NAGIOS_PLUGIN_NAME} PRIVATE ENABLE_DEFAULT_ONE_DEVICE_OPTION)
endif()

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${NAGIOS_PLUGIN_NAME}_static ${NAGIOS_SRCS})
  target_compile_definitions(${NAGIOS_PLUGIN_NAME}_static PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
  target_link_options(${NAGIOS_PLUGIN_NAME}_static PRIVATE "-static" "-L${Boost_LIBRARY_DIRS}")
  # Bypass FindBoost versions and just link explicitly with boost libraries
  # The -static link option will pick the static libraries.
  target_link_libraries(${NAGIOS_PLUGIN_NAME}_static
    PRIVATE
    xrt_core_static
    xrt_coreutil_static
    boost_system
    boost_filesystem
    boost_program_options
    -Wl,--whole-archive rt -Wl,--no-whole-archive
    )
  set_target_properties(${NAGIOS_PLUGIN_NAME}_static PROPERTIES INSTALL_RPATH "")
  install(TARGETS ${NAGIOS_PLUGIN_NAME}_static RUNTIME DESTINATION ${XRT_NAGIOS_INSTALL})
endif()

target_link_libraries(${NAGIOS_PLUGIN_NAME}
  PRIVATE
  xrt_core
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )

# Add the supporting libraries
target_link_libraries(${NAGIOS_PLUGIN_NAME} PRIVATE)

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  install (TARGETS ${NAGIOS_PLUGIN_NAME} RUNTIME DESTINATION ${XRT_NAGIOS_INSTALL})
  install (PROGRAMS ${NAGIOS_LOADER_SCRIPTS} DESTINATION ${XRT_NAGIOS_INSTALL})
endif()
