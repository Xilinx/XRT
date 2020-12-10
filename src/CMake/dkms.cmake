# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_DKMS_DRIVER_SRC_BASE_DIR
# XRT_VERSION_STRING
# LINUX_KERNEL_VERSION

set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")
set (XRT_DKMS_INSTALL_DRIVER_DIR "${XRT_DKMS_INSTALL_DIR}/driver")

message("-- XRT DRIVER SRC BASE DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}")

SET (DKMS_FILE_NAME "dkms.conf")
SET (DKMS_POSTINST "postinst")
SET (DKMS_PRERM "prerm")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/dkms-xocl/${DKMS_FILE_NAME}.in"
  ${DKMS_FILE_NAME}
  @ONLY
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  ${DKMS_POSTINST}
  @ONLY
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  ${DKMS_PRERM}
  @ONLY
  )

SET (XRT_DKMS_DRIVER_SRC_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/pcie/driver/linux)
SET (XRT_DKMS_DRIVER_INCLUDE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/pcie/driver/linux)
SET (XRT_DKMS_CORE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR})
SET (XRT_DKMS_CORE_COMMON_DRV ${XRT_DKMS_CORE_DIR}/common/drv)

SET (XRT_DKMS_DRIVER_SRCS
  xocl/mgmtpf/mgmt-core.c
  xocl/mgmtpf/mgmt-cw.c
  xocl/mgmtpf/mgmt-utils.c
  xocl/mgmtpf/mgmt-bifurcation-reset.c
  xocl/mgmtpf/mgmt-ioctl.c
  xocl/mgmtpf/mgmt-sysfs.c
  xocl/mgmtpf/mgmt-core.h
  xocl/mgmtpf/xclmgmt.dracut.conf
  xocl/mgmtpf/99-xclmgmt.rules
  xocl/mgmtpf/Makefile
  xocl/devices.h
  xocl/xocl_drv.h
  xocl/xocl_drm.h
  xocl/xocl_subdev.c
  xocl/xocl_ctx.c
  xocl/xocl_thread.c
  xocl/xocl_fdt.c
  xocl/xocl_fdt.h
  xocl/xocl_xclbin.c
  xocl/xocl_xclbin.h
  xocl/xocl_test.c
  xocl/userpf/common.h
  xocl/userpf/xocl_bo.c
  xocl/userpf/xocl_bo.h
  xocl/userpf/xocl_drm.c
  xocl/userpf/xocl_ioctl.c
  xocl/userpf/xocl_sysfs.c
  xocl/userpf/xocl_drv.c
  xocl/userpf/xocl_kds.c
  xocl/userpf/xocl.dracut.conf
  xocl/userpf/99-xocl.rules
  xocl/userpf/Makefile
  xocl/lib/libxdma.c
  xocl/lib/libxdma.h
  xocl/lib/cdev_sgdma.h
  xocl/lib/libxdma_api.h
  xocl/lib/Makefile.in
  xocl/lib/libqdma/libqdma_config.h
  xocl/lib/libqdma/libqdma_config.c
  xocl/lib/libqdma/libqdma_export.h
  xocl/lib/libqdma/libqdma_export.c
  xocl/lib/libqdma/qdma_compat.h
  xocl/lib/libqdma/qdma_context.h
  xocl/lib/libqdma/qdma_context.c
  xocl/lib/libqdma/qdma_debugfs.c
  xocl/lib/libqdma/qdma_debugfs.h
  xocl/lib/libqdma/qdma_debugfs_cmpt_queue.c
  xocl/lib/libqdma/qdma_debugfs_dev.h
  xocl/lib/libqdma/qdma_debugfs_dev.c
  xocl/lib/libqdma/qdma_debugfs_queue.h
  xocl/lib/libqdma/qdma_debugfs_queue.c
  xocl/lib/libqdma/qdma_descq.c
  xocl/lib/libqdma/qdma_descq.h
  xocl/lib/libqdma/qdma_device.c
  xocl/lib/libqdma/qdma_device.h
  xocl/lib/libqdma/qdma_intr.h
  xocl/lib/libqdma/qdma_intr.c
  xocl/lib/libqdma/qdma_context.c
  xocl/lib/libqdma/qdma_mbox.h
  xocl/lib/libqdma/qdma_qconf_mgr.h
  xocl/lib/libqdma/qdma_qconf_mgr.c
  xocl/lib/libqdma/qdma_regs.h
  xocl/lib/libqdma/qdma_regs.c
  xocl/lib/libqdma/qdma_request.h
  xocl/lib/libqdma/qdma_request.c
  xocl/lib/libqdma/qdma_st_c2h.h
  xocl/lib/libqdma/qdma_st_c2h.c
  xocl/lib/libqdma/qdma_thread.h
  xocl/lib/libqdma/qdma_thread.c
  xocl/lib/libqdma/thread.h
  xocl/lib/libqdma/thread.c
  xocl/lib/libqdma/version.h
  xocl/lib/libqdma/xdev.h
  xocl/lib/libqdma/xdev.c
  xocl/lib/libqdma4/stmc.h
  xocl/lib/libqdma4/stmc.c
  xocl/lib/libqdma4/libqdma4_export.h
  xocl/lib/libqdma4/libqdma_config.c
  xocl/lib/libqdma4/libqdma_config.h
  xocl/lib/libqdma4/libqdma_export.c
  xocl/lib/libqdma4/qdma_compat.h
  xocl/lib/libqdma4/qdma_context.c
  xocl/lib/libqdma4/qdma_context.h
  xocl/lib/libqdma4/qdma_debugfs.c
  xocl/lib/libqdma4/qdma_debugfs_dev.c
  xocl/lib/libqdma4/qdma_debugfs_dev.h
  xocl/lib/libqdma4/qdma_debugfs.h
  xocl/lib/libqdma4/qdma_debugfs_queue.c
  xocl/lib/libqdma4/qdma_debugfs_queue.h
  xocl/lib/libqdma4/qdma_descq.c
  xocl/lib/libqdma4/qdma_descq.h
  xocl/lib/libqdma4/qdma_device.c
  xocl/lib/libqdma4/qdma_device.h
  xocl/lib/libqdma4/qdma_intr.c
  xocl/lib/libqdma4/qdma_intr.h
  xocl/lib/libqdma4/qdma_sriov.c
  xocl/lib/libqdma4/qdma_license.h
  xocl/lib/libqdma4/qdma_list.c
  xocl/lib/libqdma4/qdma_list.h
  xocl/lib/libqdma4/qdma_mbox.c
  xocl/lib/libqdma4/qdma_mbox.h
  xocl/lib/libqdma4/qdma_mbox_protocol.c
  xocl/lib/libqdma4/qdma_mbox_protocol.h
  xocl/lib/libqdma4/qdma_reg_dump.h
  xocl/lib/libqdma4/qdma_regs.c
  xocl/lib/libqdma4/qdma_regs.h
  xocl/lib/libqdma4/qdma_resource_mgmt.c
  xocl/lib/libqdma4/qdma_resource_mgmt.h
  xocl/lib/libqdma4/qdma_st_c2h.c
  xocl/lib/libqdma4/qdma_st_c2h.h
  xocl/lib/libqdma4/qdma_thread.c
  xocl/lib/libqdma4/qdma_thread.h
  xocl/lib/libqdma4/qdma_ul_ext.h
  xocl/lib/libqdma4/thread.c
  xocl/lib/libqdma4/thread.h
  xocl/lib/libqdma4/version.h
  xocl/lib/libqdma4/xdev.c
  xocl/lib/libqdma4/xdev.h
  xocl/lib/libqdma4/qdma_access_common.c
  xocl/lib/libqdma4/qdma_access_common.h
  xocl/lib/libqdma4/qdma_access_errors.h
  xocl/lib/libqdma4/qdma_access_export.h
  xocl/lib/libqdma4/qdma_access_version.h
  xocl/lib/libqdma4/qdma_platform.c
  xocl/lib/libqdma4/qdma_platform_env.h
  xocl/lib/libqdma4/qdma_platform.h
  xocl/lib/libqdma4/qdma_s80_hard_access.c
  xocl/lib/libqdma4/qdma_s80_hard_access.h
  xocl/lib/libqdma4/qdma_s80_hard_reg.h
  xocl/lib/libqdma4/qdma_soft_access.c
  xocl/lib/libqdma4/qdma_soft_access.h
  xocl/lib/libqdma4/qdma_soft_reg.h
  xocl/lib/libqdma4/eqdma_soft_access.c
  xocl/lib/libqdma4/eqdma_soft_access.h
  xocl/lib/libqdma4/eqdma_soft_reg.h
  xocl/lib/libfdt/fdt.c
  xocl/lib/libfdt/fdt.h
  xocl/lib/libfdt/fdt_addresses.c
  xocl/lib/libfdt/fdt_empty_tree.c
  xocl/lib/libfdt/fdt_overlay.c
  xocl/lib/libfdt/fdt_ro.c
  xocl/lib/libfdt/fdt_rw.c
  xocl/lib/libfdt/fdt_strerror.c
  xocl/lib/libfdt/fdt_sw.c
  xocl/lib/libfdt/fdt_wip.c
  xocl/lib/libfdt/libfdt.h
  xocl/lib/libfdt/libfdt_env.h
  xocl/lib/libfdt/libfdt_internal.h
  xocl/subdev/xdma.c
  xocl/subdev/qdma.c
  xocl/subdev/qdma4.c
  xocl/subdev/feature_rom.c
  xocl/subdev/mb_scheduler.c
  xocl/subdev/xvc.c
  xocl/subdev/nifd.c
  xocl/subdev/sysmon.c
  xocl/subdev/firewall.c
  xocl/subdev/microblaze.c
  xocl/subdev/ps.c
  xocl/subdev/xiic.c
  xocl/subdev/mailbox.c
  xocl/subdev/icap.c
  xocl/subdev/clock.c
  xocl/subdev/iores.c
  xocl/subdev/axigate.c
  xocl/subdev/mig.c
  xocl/subdev/xmc.c
  xocl/subdev/xmc_u2.c
  xocl/subdev/dna.c
  xocl/subdev/fmgr.c
  xocl/subdev/mgmt_msix.c
  xocl/subdev/flash.c
  xocl/subdev/mailbox_versal.c
  xocl/subdev/ospi_versal.c
  xocl/subdev/ert.c
  xocl/subdev/aim.c
  xocl/subdev/am.c
  xocl/subdev/asm.c
  xocl/subdev/trace_fifo_lite.c
  xocl/subdev/trace_fifo_full.c
  xocl/subdev/trace_funnel.c
  xocl/subdev/trace_s2mm.c
  xocl/subdev/spc.c
  xocl/subdev/lapc.c
  xocl/subdev/memory_hbm.c
  xocl/subdev/ddr_srsr.c
  xocl/subdev/ulite.c
  xocl/subdev/calib_storage.c
  xocl/subdev/address_translator.c
  xocl/subdev/cu.c
  xocl/subdev/p2p.c
  xocl/subdev/pmc.c
  xocl/subdev/intc.c
  xocl/subdev/icap_cntrl.c
  xocl/subdev/m2m.c
  xocl/subdev/version_ctrl.c
  xocl/subdev/msix_xdma.c
  xocl/subdev/ert_user.c
  xocl/subdev/ert_30.c
  xocl/subdev/pcie_firewall.c
  xocl/Makefile
  )

# includes relative to core/pcie/driver/linux
SET (XRT_DKMS_DRIVER_INCLUDES
  include/xocl_ioctl.h
  include/mgmt-reg.h
  include/mgmt-ioctl.h
  include/qdma_ioctl.h
  include/profile_ioctl.h
  include/mailbox_proto.h
  include/flash_xrt_data.h
  )

# includes relative to core
SET (XRT_DKMS_CORE_INCLUDES
  include/ert.h
  include/xclfeatures.h
  include/xclbin.h
  include/xclerr.h
  include/xrt_mem.h
  )

SET (XRT_DKMS_COMMON_XRT_DRV
  common/drv/kds_core.c
  common/drv/xrt_cu.c
  common/drv/cu_hls.c
  common/drv/fast_adapter.c
  common/drv/cu_plram.c
  common/drv/xrt_xclbin.c
  )

SET (XRT_DKMS_COMMON_XRT_DRV_INCLUDES
  common/drv/include/xrt_drv.h
  common/drv/include/kds_core.h
  common/drv/include/kds_command.h
  common/drv/include/xrt_cu.h
  common/drv/include/xrt_xclbin.h
  )

SET (XRT_DKMS_ABS_SRCS)

foreach (DKMS_FILE ${XRT_DKMS_DRIVER_SRCS})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_DRIVER_SRC_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/${DKMS_DIR})
endforeach()
  
foreach (DKMS_FILE ${XRT_DKMS_DRIVER_INCLUDES})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_DRIVER_INCLUDE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/${DKMS_DIR})
endforeach()
  
foreach (DKMS_FILE ${XRT_DKMS_CORE_INCLUDES})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/${DKMS_DIR})
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_COMMON_XRT_DRV})
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/common/)
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_COMMON_XRT_DRV_INCLUDES})
  install (FILES ${XRT_DKMS_CORE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DRIVER_DIR}/include/)
endforeach()

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${DKMS_FILE_NAME} DESTINATION ${XRT_DKMS_INSTALL_DIR})

