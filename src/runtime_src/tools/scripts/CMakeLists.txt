if (NOT WIN32)

set(XRT_SETUP_SCRIPTS
  setup.sh
  setup.csh)

set (XRT_SCRIPTS
  xrtdeps.sh
  plp_program.sh)

install (PROGRAMS ${XRT_SCRIPTS} DESTINATION ${XRT_INSTALL_DIR}/bin)

else()

set(XRT_SETUP_SCRIPTS
  setup.bat)

endif(NOT WIN32)

install (FILES ${XRT_SETUP_SCRIPTS} DESTINATION ${XRT_INSTALL_DIR})
