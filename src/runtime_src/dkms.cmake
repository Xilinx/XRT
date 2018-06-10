set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")

SET (DKMS_FILE_NAME "dkms.conf")
SET (DKMS_POSTINST "postinst")
SET (DKMS_PRERM "prerm")

configure_file (
  "${DKMS_FILE_NAME}.in"
  ${DKMS_FILE_NAME}
  )

configure_file (
  "${DKMS_POSTINST}.in"
  ${DKMS_POSTINST}
  )

configure_file (
  "${DKMS_PRERM}.in"
  ${DKMS_PRERM}
  )


SET (XRT_DKMS_SRCS
  driver/xclng/drm/xocl/mgmtpf/mgmt-core.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-cw.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-utils.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-ioctl.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-sysfs.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-core.h
  driver/xclng/drm/xocl/mgmtpf/10-xclmgmt.rules
  driver/xclng/drm/xocl/mgmtpf/Makefile
  driver/xclng/drm/xocl/xocl_drv.h
  driver/xclng/drm/xocl/xocl_subdev.c
  driver/xclng/drm/xocl/xocl_subdev.h
  driver/xclng/drm/xocl/xocl_ctx.c
  driver/xclng/drm/xocl/xocl_thread.c
  driver/xclng/drm/xocl/xocl_test.c
  driver/xclng/drm/xocl/userpf/xdma.c
  driver/xclng/drm/xocl/userpf/qdma.c
  driver/xclng/drm/xocl/userpf/common.h
  driver/xclng/drm/xocl/userpf/xocl_bo.c
  driver/xclng/drm/xocl/userpf/xocl_bo.h
  driver/xclng/drm/xocl/userpf/xocl_drm.c
  driver/xclng/drm/xocl/userpf/xocl_drm.h
  driver/xclng/drm/xocl/userpf/xocl_ioctl.c
  driver/xclng/drm/xocl/userpf/xocl_sysfs.c
  driver/xclng/drm/xocl/userpf/xocl_drv.c
  driver/xclng/drm/xocl/userpf/10-xocl.rules
  driver/xclng/drm/xocl/userpf/Makefile
  driver/xclng/drm/xocl/lib/libxdma.c
  driver/xclng/drm/xocl/lib/libxdma.h
  driver/xclng/drm/xocl/lib/cdev_sgdma.h
  driver/xclng/drm/xocl/lib/libxdma_api.h
  driver/xclng/drm/xocl/lib/Makefile.in
  driver/xclng/drm/xocl/lib/libqdma/libqdma_export.h
  driver/xclng/drm/xocl/lib/libqdma/libqdma_export.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_descq.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_descq.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_device.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_device.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_intr.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_intr.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_context.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_context.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_mbox.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_mbox.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_sriov.c
  driver/xclng/drm/xocl/lib/libqdma/thread.c
  driver/xclng/drm/xocl/lib/libqdma/thread.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_thread.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_thread.h
  driver/xclng/drm/xocl/lib/libqdma/version.h
  driver/xclng/drm/xocl/lib/libqdma/xdev.h
  driver/xclng/drm/xocl/lib/libqdma/xdev.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_regs.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_regs.c
  driver/xclng/drm/xocl/subdev/feature_rom.c
  driver/xclng/drm/xocl/subdev/mm_xdma.c
  driver/xclng/drm/xocl/subdev/mm_qdma.c
  driver/xclng/drm/xocl/subdev/mb_scheduler.c
  driver/xclng/drm/xocl/subdev/xvc.c
  driver/xclng/drm/xocl/subdev/sysmon.c
  driver/xclng/drm/xocl/subdev/firewall.c
  driver/xclng/drm/xocl/subdev/microblaze.c
  driver/xclng/drm/xocl/subdev/xiic.c
  driver/xclng/drm/xocl/subdev/mailbox.c
  driver/xclng/drm/xocl/subdev/icap.c
  driver/xclng/drm/xocl/subdev/str_qdma.c
  driver/xclng/drm/xocl/Makefile
  driver/xclng/drm/.dir-locals.el
  driver/xclng/include/xocl_ioctl.h
  driver/xclng/include/mgmt-reg.h
  driver/xclng/include/mgmt-ioctl.h
  driver/xclng/include/qdma_ioctl.h
  driver/xclng/include/drm/drm.h
  driver/xclng/include/drm/drm_mode.h
  driver/include/ert.h
  driver/include/xclfeatures.h
  driver/include/xclbin.h
  driver/include/xclerr.h
  )

foreach (DKMS_FILE ${XRT_DKMS_SRCS})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DIR}/${DKMS_DIR})
endforeach()

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${DKMS_FILE_NAME} DESTINATION ${XRT_DKMS_INSTALL_DIR})
