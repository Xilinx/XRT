file(GLOB XMA_PLUGIN_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

set(XMA_PLUGIN_ALL_SRC
  ${XMA_PLUGIN_FILES}
  )

add_library(xmaplugin SHARED ${XMA_PLUGIN_ALL_SRC})
#target_include_directories(xmaplugin "${XML2_LIB_INCLUDE}")

set_target_properties(xmaplugin PROPERTIES VERSION ${XMA_VERSION_STRING}
  SOVERSION ${XMA_VERSION_MAJOR})

#target_link_libraries(xmaplugin "${XML2_LIB}")
target_link_libraries(xmaplugin
  xrt_core
  )

install (TARGETS xmaplugin LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib)


