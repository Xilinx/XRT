if (NOT WIN32)

set(XRT_SETUP_SCRIPTS
  setup.sh
  setup.csh)

set (XRT_SCRIPTS
  xrtdeps.sh
  plp_program.sh
  loader)

else()

set(XRT_SETUP_SCRIPTS
  setup.bat)

set (XRT_SCRIPTS
  loader.bat)

endif(NOT WIN32)

install (PROGRAMS ${XRT_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})
install (FILES ${XRT_SETUP_SCRIPTS} DESTINATION ${XRT_INSTALL_DIR})
