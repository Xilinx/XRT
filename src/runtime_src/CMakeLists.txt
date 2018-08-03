include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )

set(OPENCL_SRC "")

add_library(xilinxopencl SHARED ${OPENCL_SRC}
  $<TARGET_OBJECTS:xocl>
  $<TARGET_OBJECTS:xrt>
  $<TARGET_OBJECTS:xclbin>
  $<TARGET_OBJECTS:impl>
  )

add_library(xdp SHARED ${OPENCL_SRC}
  $<TARGET_OBJECTS:xdpobj>
  )
add_dependencies(xdp xilinxopencl)

option(XOCL_VERBOSE "Enable xocl verbosity" OFF)
option(XRT_VERBOSE "Enable xrt verbosity" OFF)

if (XOCL_VERBOSE)
  add_compile_options("-DXOCL_VERBOSE")
endif()

if(XRT_VERBOSE)
  add_compile_options("-DXRT_VERBOSE")
endif()

set_target_properties(xilinxopencl PROPERTIES LINKER_LANGUAGE CXX)
add_compile_options("-fPIC")
add_subdirectory(xdp)

add_compile_options("-Wall" "-Werror")
add_subdirectory(tools/xclbin)
add_subdirectory(impl)
add_subdirectory(xclbin)
add_subdirectory(xocl)
add_subdirectory(xrt)
add_subdirectory(ert)
add_subdirectory(tools/scripts)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

set_target_properties(xilinxopencl PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_VERSION_MAJOR})

target_link_libraries(xdp
  xilinxopencl
  )

target_link_libraries(xilinxopencl
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  dl
  pthread
  crypt
  rt
  )

install (TARGETS xilinxopencl xdp LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

add_subdirectory(driver)
