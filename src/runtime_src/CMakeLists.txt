
# Note: Add Windows compilation directives at the END of this file

if( NOT WIN32)

if(${CMAKE_BUILD_TYPE} STREQUAL "Release" AND ${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(doc)
endif()

# Default include search path starts at runtime_src
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

option(XOCL_VERBOSE "Enable xocl verbosity" OFF)
option(XRT_VERBOSE "Enable xrt verbosity" OFF)

if (XOCL_VERBOSE)
  add_compile_options("-DXOCL_VERBOSE")
endif()

if(XRT_VERBOSE)
  add_compile_options("-DXRT_VERBOSE")
endif()

add_compile_options("-fPIC")

# TODO CL_TARGET_OPENCL_VERSION is not defined..
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_compile_options("-Wall" "-Werror")
endif()
add_subdirectory(xdp)

# TODO version.h was included.
# Remove this limitation once we can generate version.h for mpsoc.
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(tools/xclbinutil)
endif()
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

add_subdirectory(core)

else()
# = WINDOWS ===================================================================

  # Enable ALL warnings and make them errors if they occur
  if (MSVC)
    # warning level 4 and all warnings are errors
    add_compile_options(/WX /W4)
    if (DEFINED MSVC_PARALLEL_JOBS)
      string(CONCAT MP_OPTION "/MP" ${MSVC_PARALLEL_JOBS})
      add_compile_options(${MP_OPTION})
    endif()
  else()
    # lots of warnings and all warnings as errors
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
    add_compile_options("-Wall" "-Werror")
  endif()

  # Build Subdirectories
  add_subdirectory(xocl)
  add_subdirectory(xrt)
  add_subdirectory(tools/xclbinutil)
  add_subdirectory(tools/scripts)
  add_subdirectory(core)
  add_subdirectory(xdp)
endif()

