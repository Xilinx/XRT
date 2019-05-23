set(XRT_XRT_CPP_HEADER_SRC
  xrt++.hpp
  xrtexec.hpp)

install (FILES ${XRT_XRT_CPP_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/experimental)

message("-- XRT xrt++ header files")
foreach (header ${XRT_XRT_CPP_HEADER_SRC})
  message("-- ${header}")
endforeach()
