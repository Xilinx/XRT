set(XRT_HEADER_SRC
  ert.h
  xcl_axi_checker_codes.h
  xclbin.h
  xclerr.h
  xclfeatures.h
  xclhal2.h
  xcl_app_debug.h
  xcl_macros.h
  xclperf.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_DIR}/include)

set(XRT_CL_EXT_SRC
  CL/cl_ext_xilinx.h)

install (FILES ${XRT_CL_EXT_SRC} DESTINATION ${XRT_INSTALL_DIR}/include/CL)

message("-- XRT header files")
foreach (header ${XRT_HEADER_SRC} ${XRT_CL_EXT_SRC})
  message("-- ${header}")
endforeach()
