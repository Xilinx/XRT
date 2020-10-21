set(XRT_EXPERIMENTAL_HEADER_SRC
  xrt-next.h
  xrt_aie.h
  xrt_graph.h
  xrt_bo.h
  xrt_device.h
  xrt_enqueue.h
  xrt_error.h
  xrt_ini.h
  xrt_kernel.h
  xrt_profile.h
  xrt_uuid.h
  xrt_xclbin.h
  xclbin_util.h
  xclbin-util.h)

install (FILES ${XRT_EXPERIMENTAL_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental COMPONENT ${XRT_DEV_COMPONENT})

message("-- XRT experimental header files")
foreach (header ${XRT_EXPERIMENTAL_HEADER_SRC})
  message("-- ${header}")
endforeach()
