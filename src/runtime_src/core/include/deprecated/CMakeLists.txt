set(XRT_DEPRECATED_HEADER_SRC
  xrt.h)

install (FILES ${XRT_DEPRECATED_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/deprecated COMPONENT ${XRT_DEV_COMPONENT})

message("-- XRT deprecated header files")
foreach (header ${XRT_DEPRECATED_HEADER_SRC})
  message("-- ${header}")
endforeach()
