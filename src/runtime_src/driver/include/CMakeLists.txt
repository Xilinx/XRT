set(XRT_HEADER_SRC
  ert.h
  xcl_axi_checker_codes.h
  xclbin.h
  xclerr.h
  xclfeatures.h
  xclhal2.h
  xcl_macros.h
  xclperf.h)

message("-- XRT header files")
foreach (header ${XRT_HEADER_SRC})
  message("-- ${header}")
endforeach()

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_DIR}/include)
