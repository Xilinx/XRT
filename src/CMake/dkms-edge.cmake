# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_DKMS_DRIVER_SRC_BASE_DIR

set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")
set (XRT_DKMS_INSTALL_DRIVER_DIR "${XRT_DKMS_INSTALL_DIR}/driver")

message("-- XRT DRIVER SRC BASE DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}")

SET (DKMS_FILE_NAME "dkms.conf")
SET (DKMS_POSTINST "postinst-edge")
SET (DKMS_PRERM "prerm-edge")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/dkms-zocl/${DKMS_FILE_NAME}.in"
  ${DKMS_FILE_NAME}
  @ONLY
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "postinst"
  @ONLY
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "prerm"
  @ONLY
  )

SET (XRT_DKMS_DRIVER_SRC_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/edge/drm)
SET (XRT_DKMS_DRIVER_INCLUDE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/edge/drm)
SET (XRT_DKMS_CORE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR})
SET (XRT_DKMS_CORE_COMMON_DRV ${XRT_DKMS_CORE_DIR}/common/drv)

SET (XRT_DKMS_DRIVER_SRCS
  zocl/include/sched_exec.h
  zocl/include/zocl_bo.h
  zocl/include/zocl_cu.h
  zocl/include/zocl_dma.h
  zocl/include/zocl_drv.h
  zocl/include/zocl_ert.h
  zocl/include/zocl_generic_cu.h
  zocl/include/zocl_ioctl.h
  zocl/include/zocl_mailbox.h
  zocl/include/zocl_ospi_versal.h
  zocl/include/zocl_sk.h
  zocl/include/zocl_util.h
  zocl/include/zocl_xclbin.h
  zocl/10-zocl.rules
  zocl/cu.c
  zocl/LICENSE
  zocl/Makefile
  zocl/sched_exec.c
  zocl/zocl_bo.c
  zocl/zocl_cu.c
  zocl/zocl_dma.c
  zocl/zocl_drv.c
  zocl/zocl_ert.c
  zocl/zocl_generic_cu.c
  zocl/zocl_ioctl.c
  zocl/zocl_kds.c
  zocl/zocl_mailbox.c
  zocl/zocl_ospi_versal.c
  zocl/zocl_ov_sysfs.c
  zocl/zocl_sk.c
  zocl/zocl_sysfs.c
  zocl/zocl_xclbin.c
  )

# includes relative to core
SET (XRT_DKMS_CORE_INCLUDES
  include/ert.h
  include/xclfeatures.h
  include/xclbin.h
  include/xclerr.h
  include/xrt_mem.h
  include/xclhal2_mem.h
  include/xrt_error_code.h
  )

SET (XRT_DKMS_COMMON_XRT_DRV
  common/drv/kds_core.c
  common/drv/xrt_cu.c
  common/drv/cu_hls.c
  common/drv/cu_plram.c
  common/drv/xrt_xclbin.c
  common/drv/Makefile
  )

SET (XRT_DKMS_COMMON_XRT_DRV_INCLUDES
  common/drv/include/xrt_drv.h
  common/drv/include/kds_core.h
  common/drv/include/kds_command.h
  common/drv/include/xrt_cu.h
  common/drv/include/xrt_xclbin.h
  )

SET (XRT_DKMS_CORE_EDGE_INCLUDES
  edge/include/zynq_ioctl.h
  )

foreach (DKMS_FILE ${XRT_DKMS_DRIVER_SRCS})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_DRIVER_SRC_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/${DKMS_DIR})
endforeach()
  
foreach (DKMS_FILE ${XRT_DKMS_CORE_INCLUDES})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/zocl/${DKMS_DIR})
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_COMMON_XRT_DRV})
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/common/)
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_COMMON_XRT_DRV_INCLUDES})
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/zocl/include/)
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_CORE_EDGE_INCLUDES})
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/zocl/include/)
endforeach()

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${DKMS_FILE_NAME} DESTINATION ${XRT_DKMS_INSTALL_DIR})

