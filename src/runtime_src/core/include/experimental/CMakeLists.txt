set(XRT_EXPERIMENTAL_HEADER_SRC
  xrt-next.h
  xrt_aie.h
  xrt_kernel.h
  xclbin-util.h)

install (FILES ${XRT_EXPERIMENTAL_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental)

message("-- XRT experimental header files")
foreach (header ${XRT_EXPERIMENTAL_HEADER_SRC})
  message("-- ${header}")
endforeach()
