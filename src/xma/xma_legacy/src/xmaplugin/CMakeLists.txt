file(GLOB XMA_PLUGIN_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

set(XMA_PLUGIN_ALL_SRC
  ${XMA_PLUGIN_FILES}
  )

add_library(xmaplugin_legacy SHARED ${XMA_PLUGIN_ALL_SRC})
#target_include_directories(xmaplugin "${XML2_LIB_INCLUDE}")

set_target_properties(xmaplugin_legacy PROPERTIES VERSION ${XMA_VERSION_STRING}
  SOVERSION ${XMA_SOVERSION})

#target_link_libraries(xmaplugin "${XML2_LIB}")
#Removed xrt_core library as name is different on aws
#target_link_libraries(xmaplugin
#  xrt_core
#  )

install (TARGETS xmaplugin_legacy LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib)

#CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libxmaplugin.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/libxmaplugin.pc @ONLY)
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libxmaplugin.pc DESTINATION ${XMA_INSTALL_DIR}/lib/pkgconfig)
