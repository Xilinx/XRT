set(XRT_EDGE_EXPERIMENTAL_HEADER_SRC
  xrt_aie.h)

install (FILES ${XRT_EDGE_EXPERIMENTAL_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental)

message("-- XRT experimental header files")
foreach (header ${XRT_EDGE_EXPERIMENTAL_HEADER_SRC})
  message("-- ${header}")
endforeach()
