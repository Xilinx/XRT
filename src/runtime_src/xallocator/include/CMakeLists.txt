set(XALLOCATOR_HEADER_SRC
  xallocator.h)

install (FILES ${XALLOCATOR_HEADER_SRC} DESTINATION ${XRT_INSTALL_DIR}/include/xrt)

message("-- XRT header files")
foreach (header ${XALLOCATOR_HEADER_SRC})
  message("-- ${header}")
endforeach()
