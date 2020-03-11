if( NOT WIN32)
  set(XRT_LOADER_SCRIPTS
    loader
    xbutil
    xbmgmt)
else()
  set(XRT_LOADER_SCRIPTS
    loader.bat
    xbutil
    xbutil.bat
    xbmgmt
    xbmgmt.bat)
endif()

install (PROGRAMS ${XRT_LOADER_SCRIPTS} DESTINATION ${XRT_INSTALL_BIN_DIR})

