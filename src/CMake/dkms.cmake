# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_DKMS_DRIVER_SRC_BASE_DIR

set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")

message("-- XRT DRIVER SRC BASE DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}")

SET (DKMS_FILE_NAME "dkms.conf")
SET (DKMS_POSTINST "postinst")
SET (DKMS_PRERM "prerm")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/dkms-xocl/${DKMS_FILE_NAME}.in"
  ${DKMS_FILE_NAME}
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  ${DKMS_POSTINST}
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  ${DKMS_PRERM}
  )

SET (XRT_DKMS_SRCS
  driver/xclng/drm/xocl/mgmtpf/mgmt-core.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-cw.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-utils.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-ioctl.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-sysfs.c
  driver/xclng/drm/xocl/mgmtpf/mgmt-core.h
  driver/xclng/drm/xocl/mgmtpf/xclmgmt.dracut.conf
  driver/xclng/drm/xocl/mgmtpf/10-xclmgmt.rules
  driver/xclng/drm/xocl/mgmtpf/Makefile
  driver/xclng/drm/xocl/devices.h
  driver/xclng/drm/xocl/xocl_drv.h
  driver/xclng/drm/xocl/xocl_drm.h
  driver/xclng/drm/xocl/xocl_subdev.c
  driver/xclng/drm/xocl/xocl_ctx.c
  driver/xclng/drm/xocl/xocl_thread.c
  driver/xclng/drm/xocl/xocl_test.c
  driver/xclng/drm/xocl/userpf/common.h
  driver/xclng/drm/xocl/userpf/xocl_bo.c
  driver/xclng/drm/xocl/userpf/xocl_bo.h
  driver/xclng/drm/xocl/userpf/xocl_drm.c
  driver/xclng/drm/xocl/userpf/xocl_ioctl.c
  driver/xclng/drm/xocl/userpf/xocl_sysfs.c
  driver/xclng/drm/xocl/userpf/xocl_drv.c
  driver/xclng/drm/xocl/userpf/xocl.dracut.conf
  driver/xclng/drm/xocl/userpf/10-xocl.rules
  driver/xclng/drm/xocl/userpf/Makefile
  driver/xclng/drm/xocl/lib/libxdma.c
  driver/xclng/drm/xocl/lib/libxdma.h
  driver/xclng/drm/xocl/lib/cdev_sgdma.h
  driver/xclng/drm/xocl/lib/libxdma_api.h
  driver/xclng/drm/xocl/lib/Makefile.in
  driver/xclng/drm/xocl/lib/libqdma/libqdma_config.h
  driver/xclng/drm/xocl/lib/libqdma/libqdma_config.c
  driver/xclng/drm/xocl/lib/libqdma/libqdma_export.h
  driver/xclng/drm/xocl/lib/libqdma/libqdma_export.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_compat.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_context.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_context.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs_cmpt_queue.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs_dev.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs_dev.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs_queue.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_debugfs_queue.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_descq.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_descq.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_device.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_device.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_intr.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_intr.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_context.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_mbox.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_qconf_mgr.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_qconf_mgr.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_regs.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_regs.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_request.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_request.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_st_c2h.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_st_c2h.c
  driver/xclng/drm/xocl/lib/libqdma/qdma_thread.h
  driver/xclng/drm/xocl/lib/libqdma/qdma_thread.c
  driver/xclng/drm/xocl/lib/libqdma/thread.h
  driver/xclng/drm/xocl/lib/libqdma/thread.c
  driver/xclng/drm/xocl/lib/libqdma/version.h
  driver/xclng/drm/xocl/lib/libqdma/xdev.h
  driver/xclng/drm/xocl/lib/libqdma/xdev.c
  driver/xclng/drm/xocl/subdev/xdma.c
  driver/xclng/drm/xocl/subdev/qdma.c
  driver/xclng/drm/xocl/subdev/feature_rom.c
  driver/xclng/drm/xocl/subdev/mb_scheduler.c
  driver/xclng/drm/xocl/subdev/xvc.c
  driver/xclng/drm/xocl/subdev/sysmon.c
  driver/xclng/drm/xocl/subdev/firewall.c
  driver/xclng/drm/xocl/subdev/microblaze.c
  driver/xclng/drm/xocl/subdev/xiic.c
  driver/xclng/drm/xocl/subdev/mailbox.c
  driver/xclng/drm/xocl/subdev/icap.c
  driver/xclng/drm/xocl/subdev/mig.c
  driver/xclng/drm/xocl/subdev/xmc.c
  driver/xclng/drm/xocl/subdev/dna.c
  driver/xclng/drm/xocl/subdev/fmgr.c
  driver/xclng/drm/xocl/Makefile
  driver/xclng/drm/.dir-locals.el
  driver/xclng/include/xocl_ioctl.h
  driver/xclng/include/mgmt-reg.h
  driver/xclng/include/mgmt-ioctl.h
  driver/xclng/include/qdma_ioctl.h
  driver/include/ert.h
  driver/include/xclfeatures.h
  driver/include/xclbin.h
  driver/include/xclerr.h
  )

SET (XRT_DKMS_ABS_SRCS)

foreach (DKMS_FILE ${XRT_DKMS_SRCS})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install (FILES ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DIR}/${DKMS_DIR})
  list (APPEND XRT_DKMS_ABS_SRCS ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/${DKMS_FILE})
endforeach()

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${DKMS_FILE_NAME} DESTINATION ${XRT_DKMS_INSTALL_DIR})

find_program(CHECKPATCH checkpatch.pl PATHS /lib/modules/${LINUX_KERNEL_VERSION}/build/scripts/ NO_DEFAULT_PATH)

message("-- checkpatch ${CHECKPATCH}")

if (NOT CHECKPATCH)
  message (WARNING "-- checkpatch.pl not found, Linux driver code style check disabled")
else ()
  add_custom_target(
    checkpatch
    COMMAND ${CHECKPATCH}
    --emacs
    --no-tree -f
    ${XRT_DKMS_ABS_SRCS}
    )
endif ()
