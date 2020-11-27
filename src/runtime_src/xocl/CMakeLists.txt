include_directories(
  ${Boost_INCLUDE_DIRS}
  ${XRT_SRC_DIR}/include/1_2
)

set(XRT_XOCL_API_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/api")
set(XRT_XOCL_CORE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(XRT_XOCL_XCLBIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/xclbin")

include_directories(
  ${XRT_XOCL_API_DIR}
)

file(GLOB XRT_XOCL_API_FILES
  "${XRT_XOCL_API_DIR}/*.h"
  "${XRT_XOCL_API_DIR}/*.cpp"
  "${XRT_XOCL_API_DIR}/detail/*.h"
  "${XRT_XOCL_API_DIR}/detail/*.cpp"
  "${XRT_XOCL_API_DIR}/icd/*.cpp"
  "${XRT_XOCL_API_DIR}/khronos/*.h"
  "${XRT_XOCL_API_DIR}/khronos/*.cpp"
  "${XRT_XOCL_API_DIR}/xlnx/*.h"
  "${XRT_XOCL_API_DIR}/xlnx/*.cpp"
  "${XRT_XOCL_API_DIR}/printf/*.h"
  "${XRT_XOCL_API_DIR}/printf/*.cpp"
  "${XRT_XOCL_API_DIR}/plugin/xdp/*.h"
  "${XRT_XOCL_API_DIR}/plugin/xdp/*.cpp"
  )

file(GLOB XRT_XOCL_CORE_FILES
  "${XRT_XOCL_CORE_DIR}/*.h"
  "${XRT_XOCL_CORE_DIR}/*.cpp"
  )

file(GLOB XRT_XOCL_XCLBIN_FILES
  "${XRT_XOCL_XCLBIN_DIR}/*.h"
  "${XRT_XOCL_XCLBIN_DIR}/*.cpp"
  )

set(XRT_XOCL_ALL_SRC
  ${XRT_XOCL_API_FILES}
  ${XRT_XOCL_CORE_FILES}
  ${XRT_XOCL_XCLBIN_FILES}
  )

add_compile_options("-DXRT_XOCL_SOURCE")

add_library(xocl OBJECT ${XRT_XOCL_ALL_SRC})

if (WIN32)
include_directories(
  ${KHRONOS}/include
  )

add_library(xilinxopencl SHARED
  $<TARGET_OBJECTS:xocl>
  ${XRT_XOCL_API_DIR}/icd/windows/xilinxopencl.def
  )

target_link_libraries(xilinxopencl
  PRIVATE
  xrt++
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

else ()

add_library(xilinxopencl SHARED
  $<TARGET_OBJECTS:xocl>
  )

# Attach to the user's linker flags
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")

set_target_properties(xilinxopencl
  PROPERTIES LINKER_LANGUAGE CXX
  )

target_link_libraries(xilinxopencl
  PRIVATE
  xrt++
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  dl
  pthread
  crypt
  uuid
  rt
  )

endif ()

# install(TARGETS xilinxopencl
#   ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
#   LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT xrt NAMELINK_COMPONENT ${XRT_DEV_COMPONENT}
#   RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
# )

install(TARGETS xilinxopencl
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xilinxopencl
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
