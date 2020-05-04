if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

file(GLOB CONTAINER_PLUGIN_FILES
  "container.h"
  "container.cpp"
  )

add_library(container_plugin OBJECT ${CONTAINER_PLUGIN_FILES})

add_library(container_mpd_plugin SHARED ""
  $<TARGET_OBJECTS:container_plugin>
)

add_compile_options("-fPIC")

target_link_libraries(container_mpd_plugin
  xrt_core_static
  xrt_coreutil_static
  uuid
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  pthread
  rt
  crypto
  )

install(TARGETS container_mpd_plugin
  LIBRARY
  DESTINATION ${XRT_INSTALL_LIB_DIR}
  COMPONENT container
  )
endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
