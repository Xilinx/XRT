include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  )

file(GLOB MPD_FILES
  "mpd.cpp"
  "common.h"
  )
set(MPD_SRC ${MPD_FILES})
add_executable(mpd ${MPD_SRC})
target_link_libraries(mpd
  xrt_core
  pthread
  uuid
  )
install (TARGETS mpd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

file(GLOB MSD_FILES
  "msd.cpp"
  "common.h"
  )
set(MSD_SRC ${MSD_FILES})
add_executable(msd ${MSD_SRC})
target_link_libraries(msd
  xrt_core
  pthread
  uuid
  )
install (TARGETS msd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

# deploy the config file
install (FILES msd-host.config DESTINATION ${XRT_INSTALL_DIR}/etc)
