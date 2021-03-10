set(XRT_HEADER_SRC
  ert.h
  xstream.h
  types.h
  xcl_axi_checker_codes.h
  xclbin.h
  xclerr.h
  xclfeatures.h
  xclhal2.h
  xrt.h
  xcl_app_debug.h
  xcl_macros.h
  xcl_app_debug.h
  xclperf.h
  xclhal2_mem.h
  xrt_mem.h
  xrt_error_code.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR} COMPONENT ${XRT_DEV_COMPONENT})

add_subdirectory(experimental)
add_subdirectory(deprecated)
if(WIN32)
add_subdirectory(windows)
endif()
