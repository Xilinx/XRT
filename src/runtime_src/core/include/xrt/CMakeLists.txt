set(XRT_XRT_HEADER_SRC
  xrt_bo.h
  xrt_device.h
  xrt_kernel.h
  xrt_uuid.h)


install (FILES ${XRT_XRT_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt COMPONENT ${XRT_DEV_COMPONENT})

set(XRT_XRT_DETAIL_HEADER_SRC
  detail/param_traits.h
  detail/pimpl.h)
  
install (FILES ${XRT_XRT_DETAIL_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail COMPONENT ${XRT_DEV_COMPONENT})
