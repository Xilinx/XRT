file(GLOB XMA_API_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

set(XMA_API_ALL_SRC
  ${XMA_API_FILES}
  )

find_package(LibXml2 2.9.1 REQUIRED)
#yaml doesn't have cmake setup files
#find_package(libyaml-0-2 0.1.4 REQUIRED)

find_library(XML2_LIB xml2)
find_path(XML2_LIB_INCLUDE NAMES "libxml/parser.h" PATH_SUFFIXES "libxml2")
if(NOT XML2_LIB_INCLUDE)
  message(ERROR ": xml2 headers not found")
  set(XML2_LIB_INCLUDE "/usr/include/libxml2")
  MESSAGE("xml2 include path: ${XML2_LIB_INCLUDE}")
endif()

add_library(xmaapi SHARED ${XMA_API_ALL_SRC})
target_include_directories(xmaapi PRIVATE
  "${OPENCL_INCLUDEDIR}"
  "${XML2_LIB_INCLUDE}")
#  "/usr/include/libxml2"

set_target_properties(xmaapi PROPERTIES VERSION ${XMA_VERSION_STRING}
  SOVERSION ${XMA_SOVERSION})

#target_link_libraries(xmaapi "${XML2_LIB}")
#removed xrt_core lib as name is different for aws and xbb
target_link_libraries(xmaapi
  m
  dl
  gcc_s
  stdc++
  xml2
  yaml
  )

install(TARGETS xmaapi LIBRARY DESTINATION ${XMA_INSTALL_DIR}/lib)

#CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libxmaapi.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/libxmaapi.pc @ONLY)
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libxmaapi.pc DESTINATION ${XMA_INSTALL_DIR}/lib/pkgconfig)
