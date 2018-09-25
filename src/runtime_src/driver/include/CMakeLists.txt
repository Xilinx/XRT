set(XRT_HEADER_SRC
  ert.h
  stream.h
  xcl_axi_checker_codes.h
  xclbin.h
  xclerr.h
  xclfeatures.h
  xclhal2.h
  xcl_app_debug.h
  xcl_macros.h
  xclperf.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_DIR}/include)

message("-- XRT header files")
foreach (header ${XRT_HEADER_SRC})
  message("-- ${header}")
endforeach()
