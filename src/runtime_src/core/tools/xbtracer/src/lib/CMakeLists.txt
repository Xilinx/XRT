link_directories($ENV{XILINX_XRT}/lib)

add_library(xrt_capture SHARED
  capture.cpp
  logger.cpp
  xrt_device_inst.cpp
)

if (NOT WIN32)
  target_link_libraries(xrt_capture PRIVATE uuid)
endif(NOT WIN32)

install(TARGETS xrt_capture
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_capture
  EXPORT xrt-dev-targets
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
