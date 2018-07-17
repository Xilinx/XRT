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

PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER ${PROTO_SRC_FILES})
set(PROTOBUF_PROTOC_EXECUTABLE "protoc")

exec_program(${PROTOBUF_PROTOC_EXECUTABLE}
  ARGS --cpp_out  ${CMAKE_CURRENT_BINARY_DIR} --proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_SRC_FILES})

# Generate Cpp files from Proto is done

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )

file(GLOB COMMON_EM_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  "${CMAKE_CURRENT_BINARY_DIR}/*.cc"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(common_em_objects OBJECT ${COMMON_EM_SRC_FILES})

set(CURR_SOURCE "")
add_library(common_em SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:common_em_objects>
  )

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

target_link_libraries(common_em
  ${BOOST_FILESYSTEM_LIBRARY}
  ${BOOST_SYSTEM_LIBRARY}
  ${PROTOBUF_LIBRARY}
  dl
  pthread
  crypt
  rt
  )

install (TARGETS common_em LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
