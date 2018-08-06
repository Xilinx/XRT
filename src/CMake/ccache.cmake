if (RDI_CCACHE)

  find_program(gccwrap /home/soeren/perforce/sbx-p4/HEAD/src/misc/compiler/gccwrap/lnx64/bin/gccwrap)
  find_program(gccarwrap /home/soeren/perforce/sbx-p4/HEAD/src/misc/compiler/gccwrap/lnx64/bin/gccarwrap)

  if (gccwrap)
    message ("-- Using compile cache from $ENV{RDI_CCACHEROOT}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${gccwrap} --with-cache-rw")
    #  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${gccarwrap} --with-cache-rw")
  endif ()

endif()
