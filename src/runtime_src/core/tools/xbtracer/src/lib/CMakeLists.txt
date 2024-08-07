link_directories($ENV{XILINX_XRT}/lib)

add_library(xrt_capture SHARED
  logger.cpp
)

install(TARGETS xrt_capture)
