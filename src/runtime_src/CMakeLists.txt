if(${CMAKE_BUILD_TYPE} STREQUAL "Release" AND ${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(doc)
endif()

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

add_library(oclxdp SHARED ${OPENCL_SRC}
  $<TARGET_OBJECTS:xdpobj>
  )
add_dependencies(oclxdp xdp xilinxopencl)

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

# TODO CL_TARGET_OPENCL_VERSION is not defined..
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_compile_options("-Wall" "-Werror")
endif()
add_subdirectory(xdp)

# TODO version.h was included.
# Remove this limitation once we can generate version.h for mpsoc.
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(tools/xclbin)
endif()
add_subdirectory(impl)
add_subdirectory(xclbin)
add_subdirectory(xocl)
add_subdirectory(xrt)
add_subdirectory(ert)
add_subdirectory(tools/scripts)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

set_target_properties(xilinxopencl PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(oclxdp
  xdp
  xilinxopencl
 )

target_link_libraries(xilinxopencl
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  dl
  pthread
  crypt
  uuid
  rt
  )

install (TARGETS xilinxopencl oclxdp LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

add_subdirectory(driver)
