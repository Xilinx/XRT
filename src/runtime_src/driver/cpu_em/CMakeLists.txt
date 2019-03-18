set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generic_pcie_hal2")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../common_em")
set(COMMON_EM_GEN_DIR  "${CMAKE_CURRENT_BINARY_DIR}/../common_em")

include_directories(
  ${EM_SRC_DIR}
  ${COMMON_EM_SRC_DIR}
  ${COMMON_EM_GEN_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )


file(GLOB EM_SRC_FILES
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cxx"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(sw_emu_objects OBJECT ${EM_SRC_FILES})

set(CURR_SOURCE "")
add_library(xrt_swemu SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sw_emu_objects>
  )

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

set_target_properties(xrt_swemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_swemu
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${PROTOBUF_LIBRARY}
  common_em
  rt
  )

install (TARGETS xrt_swemu LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
