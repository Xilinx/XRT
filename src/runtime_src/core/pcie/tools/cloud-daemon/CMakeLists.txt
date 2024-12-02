# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

file(GLOB MPD_FILES
  "mpd.cpp"
  "pciefunc.cpp"
  "pciefunc.h"
  "common.cpp"
  "common.h"
  "sw_msg.cpp"
  "sw_msg.h"
  "mpd_plugin.h"
  )
set(MPD_SRC ${MPD_FILES})
add_executable(mpd ${MPD_SRC})
target_link_libraries(mpd
  PRIVATE
  xrt_core_static
  xrt_coreutil_static
  pthread
  uuid
  dl
  udev
  )
install (TARGETS mpd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

file(GLOB MSD_FILES
  "msd.cpp"
  "pciefunc.cpp"
  "pciefunc.h"
  "common.cpp"
  "common.h"
  "sw_msg.cpp"
  "sw_msg.h"
  "msd_plugin.h"
  )
set(MSD_SRC ${MSD_FILES})
add_executable(msd ${MSD_SRC})
target_link_libraries(msd
  PRIVATE
  xrt_core_static
  xrt_coreutil_static
  pthread
  uuid
  dl
  )
install (TARGETS msd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# deploy the systemd unit file for msd/mpd
set (MSD_SERVICE_FILE "msd.service")
configure_file (
  ${MSD_SERVICE_FILE}.in
  ${MSD_SERVICE_FILE}
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${MSD_SERVICE_FILE}
  DESTINATION ${XRT_INSTALL_DIR}/etc
  )
set (MPD_SERVICE_FILE "mpd.service")
configure_file (
  ${MPD_SERVICE_FILE}.in
  ${MPD_SERVICE_FILE}
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${MPD_SERVICE_FILE}
  DESTINATION ${XRT_INSTALL_DIR}/etc
  )

# depoly header files
set(XRT_HEADER_SRC
  mpd_plugin.h                                                              
  msd_plugin.h)                                                              
                                                                                
install (FILES ${XRT_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
  COMPONENT ${XRT_DEV_COMPONENT}
)        

if (XRT_ALVEO)
  add_subdirectory(aws)
  add_subdirectory(azure)
  add_subdirectory(container)
endif()
