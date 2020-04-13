SET (Protobuf_DEBUG)
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

include_directories (
${PROTOBUF_INCLUDE_DIR}
)
# Generate Cpp files from Proto file
file(GLOB PROTO_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.proto"
  )

PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${PROTO_SRC_FILES})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )

add_custom_target(generated_code DEPENDS ${ProtoSources} ${ProtoHeaders})
file(GLOB COMMON_EM_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(common_em_objects OBJECT ${COMMON_EM_SRC_FILES} ${ProtoHeaders} ${ProtoSources})

set(CURR_SOURCE "")
add_library(common_em SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:common_em_objects>
  )

add_dependencies(common_em generated_code)

set_target_properties(common_em PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(common_em
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${PROTOBUF_LIBRARY}
  xrt_coreutil
  dl
  pthread
  crypt
  rt
  )

install (TARGETS common_em LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR})
