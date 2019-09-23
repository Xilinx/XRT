
# Note: Add Windows compilation directives at the END of this file

if( NOT WIN32)

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
  )

add_library(oclxdp SHARED ${OPENCL_SRC}
  $<TARGET_OBJECTS:xdpobj>
  )
add_dependencies(oclxdp xdp xilinxopencl)

add_library(xdp_hal_plugin SHARED 
  $<TARGET_OBJECTS:xdp_hal_plugin_obj>
  )

add_dependencies(xdp_hal_plugin xdp)

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
add_subdirectory(xclbin)
add_subdirectory(xocl)
add_subdirectory(xrt)
add_subdirectory(ert)
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  # Only for host native build.
  # For embedded, headers and libraries are installed in /usr
  # Not requeired setup.sh/.csh
  add_subdirectory(tools/scripts)
endif()

# Attach to the user's linker flags
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")

set_target_properties(xilinxopencl PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  INSTALL_RPATH $ORIGIN
  )

target_link_libraries(oclxdp
  xdp
  xilinxopencl
 )

target_link_libraries(xdp_hal_plugin xdp)

set_target_properties(xdp_hal_plugin PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

set_target_properties(oclxdp PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

set_target_properties(xdp PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xilinxopencl
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  dl
  pthread
  crypt
  uuid
  rt
  )

install (TARGETS xilinxopencl oclxdp xdp_hal_plugin LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

add_subdirectory(core)

else()
# = WINDOWS ===================================================================

  # Enable ALL warnings and make them errors if they occur
  if (MSVC)
    # warning level 4 and all warnings are errors
    add_compile_options(/W4 /WX)
  else()
    # lots of warnings and all warnings as errors
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
    add_compile_options("-Wall" "-Werror")
  endif()


  # Build Subdirectories
  add_subdirectory(tools/xclbin)
endif()

