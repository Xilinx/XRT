if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

file(GLOB AZURE_PLUGIN_FILES
  "azure.h"
  "azure.cpp"
  "../common.h"
  "../common.cpp"
  "../sw_msg.h"
  "../sw_msg.cpp"
  "../pciefunc.h"
  "../pciefunc.cpp"
  )

add_library(azure_plugin OBJECT ${AZURE_PLUGIN_FILES})

add_library(azure_mpd_plugin SHARED ""
  $<TARGET_OBJECTS:azure_plugin>
)

add_compile_options("-fPIC")

target_link_libraries(azure_mpd_plugin
  xrt_core_static
  xrt_coreutil_static
  uuid
  boost_filesystem
  boost_system
  pthread
  rt
  dl
  curl
  crypto
  )

install(TARGETS azure_mpd_plugin
  LIBRARY
  DESTINATION ${XRT_INSTALL_LIB_DIR}
  COMPONENT azure
  )

endif (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
