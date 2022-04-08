# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#

# This plugin should be compiled only for Linux distributions

# Collect local directory files
file(GLOB NAGIOS_FILES
  "NagiosPlugin.cpp"
)

# Determine the name of the executable
set(NAGIOS_PLUGIN_NAME "xrt_nagios_plugin")


add_executable(${NAGIOS_PLUGIN_NAME} ${NAGIOS_FILES})

# Determine what functionality should be added
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  target_compile_definitions(${NAGIOS_PLUGIN_NAME} PRIVATE ENABLE_NATIVE_SUBCMDS_AND_REPORTS)
else()
  target_compile_definitions(${NAGIOS_PLUGIN_NAME} PRIVATE ENABLE_DEFAULT_ONE_DEVICE_OPTION)
endif()

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${NAGIOS_PLUGIN_NAME}_static ${NAGIOS_FILES})
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
  install(TARGETS ${NAGIOS_PLUGIN_NAME}_static RUNTIME DESTINATION "${XRT_INSTALL_DIR}/etc/nagios-plugins")
endif()

target_link_libraries(${NAGIOS_PLUGIN_NAME}
  PRIVATE
  xrt_core
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  )

target_link_libraries(${NAGIOS_PLUGIN_NAME} PRIVATE)

# Package xrt sub commands json file for embedded builds
# This file acts as sample json file and will be removed in future releases
if (${XRT_NATIVE_BUILD} STREQUAL "no")
  set(XRT_SUBCOMMANDS_JSON ../common/xrt_subcommands.json)
  install(FILES ${XRT_SUBCOMMANDS_JSON} DESTINATION ${XRT_INSTALL_INCLUDE_DIR})
endif()

# Install our built executable
install (TARGETS ${NAGIOS_PLUGIN_NAME} RUNTIME DESTINATION "${XRT_INSTALL_DIR}/etc/nagios-plugins")
install (PROGRAMS "./nagios_plugin.sh" DESTINATION "${XRT_INSTALL_DIR}/etc/nagios-plugins")
# -----------------------------------------------------------------------------
