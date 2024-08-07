link_directories($ENV{XILINX_XRT}/lib)

add_library(xrt_capture SHARED
  capture.cpp
  logger.cpp
  xrt_device_inst.cpp
)

if (NOT WIN32)
  target_link_libraries(xrt_capture PRIVATE uuid)
endif(NOT WIN32)

install(TARGETS xrt_capture)
