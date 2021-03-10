set(XRT_HEADER_SRC
  mailbox_proto.h)

install (FILES ${XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR} COMPONENT ${XRT_DEV_COMPONENT})

message("-- XRT header files")
foreach (header ${XRT_HEADER_SRC})
  message("-- ${header}")
endforeach()
