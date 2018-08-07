if (RDI_CCACHE)

  find_program(gccwrap /tools/batonroot/rodin/devkits/lnx64/ccwrap/gccwrap)
  find_program(gccarwrap /tools/batonroot/rodin/devkits/lnx64/ccwrap/gccarwrap)

  if (gccwrap)
    message ("-- Using compile cache wrapper ${gccwrap} with cache in $ENV{RDI_CCACHEROOT}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${gccwrap} --with-cache-rw")
  endif ()

  if (gccarwrap)
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> -qc -o <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> -qc -o <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH)
    set(CMAKE_C_ARCHIVE_FINISH)
    message ("-- Using link cache wrapper ${gccarwrap} with cache in $ENV{RDI_CCACHEROOT}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${gccarwrap} --with-cache-rw")
  endif ()

endif()
