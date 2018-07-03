set(CPU_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generic_pcie_hal2")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../common_em")
set(COMMON_EM_GEN_DIR  "${CMAKE_CURRENT_BINARY_DIR}/../common_em")

include_directories(
  ${CPU_EM_SRC_DIR}
  ${COMMON_EM_SRC_DIR}
  ${COMMON_EM_GEN_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )


file(GLOB CPU_EM_SRC_FILES
  "${CPU_EM_SRC_DIR}/*.h"
  "${CPU_EM_SRC_DIR}/*.cxx"
  )

add_definitions(-DUSE_HAL2 -DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(cpu_em_objects OBJECT ${CPU_EM_SRC_FILES})

set(CURR_SOURCE "")
add_library(cpu_em SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:cpu_em_objects>
  )

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

target_link_libraries(cpu_em
  ${BOOST_FILESYSTEM_LIBRARY}
  ${BOOST_SYSTEM_LIBRARY}
  ${PROTOBUF_LIBRARY}
  common_em
  rt
  )

install (TARGETS cpu_em LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

