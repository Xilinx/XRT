# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(XRT_RUNTIME_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(XRT_DOC_TOC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/toc")
set(DOC_CORE_DIR           "${CMAKE_CURRENT_BINARY_DIR}/core")
set(DOC_TOC_DIR            "${CMAKE_CURRENT_BINARY_DIR}/toc")

file(GLOB XRT_MGMT_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/core/pcie/driver/linux/include/mgmt-ioctl.h)
file(GLOB XRT_XOCL_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/core/pcie/driver/linux/include/xocl_ioctl.h)
file(GLOB XRT_ZOCL_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/core/edge/include/zynq_ioctl.h)
file(GLOB XRT_BO_H         ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_bo.h)
file(GLOB XRT_DEVICE_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_device.h)
file(GLOB XRT_IP_H         ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ip.h)
file(GLOB XRT_KERNEL_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_kernel.h)
file(GLOB XRT_MAILBOX_H    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_mailbox.h)
file(GLOB XRT_MESSAGE_H    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_message.h)
#file(GLOB XRT_QUEUE_H      ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_queue.h)
file(GLOB XRT_SYSTEM_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_system.h)
file(GLOB XRT_UUID_H       ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_uuid.h)
file(GLOB XRT_XCLBIN_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_xclbin.h)
file(GLOB XRT_INI_H        ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ini.h)
file(GLOB XRT_ERT_H        ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/detail/ert.h)
file(GLOB XRT_MAILBOX_PROTO_H ${XRT_RUNTIME_SRC_DIR}/core/pcie/driver/linux/include/mailbox_proto.h)
file(GLOB XRT_MAILBOX_C    ${XRT_RUNTIME_SRC_DIR}/core/pcie/driver/linux/xocl/subdev/mailbox.c)

file(MAKE_DIRECTORY ${DOC_CORE_DIR})
file(COPY ${XRT_DOC_TOC_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(KERNELDOC "${CMAKE_BINARY_DIR}/kernel-doc")
set(KERNELDOC_URL "https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/plain/scripts/kernel-doc?h=v4.14.52")

if (NOT EXISTS ${KERNELDOC})
  MESSAGE(STATUS "${KERNELDOC} downloading")
  file(DOWNLOAD ${KERNELDOC_URL} ${KERNELDOC})
  execute_process(COMMAND chmod +x ${KERNELDOC})
endif ()

find_program(KERNELDOC_EXECUTABLE ${KERNELDOC} PATHS ${CMAKE_BINARY_DIR})
find_program(SPHINX_EXECUTABLE sphinx-build)
find_program(DOXYGEN_EXECUTABLE doxygen)

if (NOT DOXYGEN_EXECUTABLE)
  message(WARNING "doxygen not found, XRT C++ documentation is disabled")
  add_custom_target(
    doxygen
    )
else()
  set(XRT_CPP_APIS
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_device.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_bo.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_kernel.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_uuid.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_ini.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_mailbox.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_message.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_ip.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_system.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/experimental/xrt_xclbin.h
    )
  string(REPLACE ";" " " DOXY_INPUT "${XRT_CPP_APIS}")
  set(DOXYFILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
  add_custom_target(
    doxygen
    DEPENDS ${XRT_CPP_APIS}
    COMMAND ${DOXYGEN_EXECUTABLE} -g ${DOXYFILE}
    COMMAND echo "PROJECT_NAME = XRT" >> ${DOXYFILE}
    COMMAND echo "GENERATE_XML = YES" >> ${DOXYFILE}
    COMMAND echo "MACRO_EXPANSION = YES" >> ${DOXYFILE}
    COMMAND echo "EXTRACT_ALL = YES" >> ${DOXYFILE}
    COMMAND echo "SHOW_NAMESPACES = YES" >> ${DOXYFILE}
    COMMAND echo "PREDEFINED = __cplusplus XCL_DRIVER_DLLESPEC=" >> ${DOXYFILE}
    COMMAND echo "EXPAND_AS_DEFINED = XCL_DRIVER_DLLESPEC" >> ${DOXYFILE}
    COMMAND echo "IMAGE_PATH = ${XRT_DOC_TOC_DIR}" >> ${DOXYFILE}
    COMMAND echo "INPUT = ${DOXY_INPUT}" >> ${DOXYFILE}
    COMMAND ${DOXYGEN_EXECUTABLE}
    )
endif(NOT DOXYGEN_EXECUTABLE)

if (NOT KERNELDOC_EXECUTABLE OR NOT SPHINX_EXECUTABLE)
  MESSAGE (WARNING "kernel-doc or Sphinx not found, XRT documentation build disabled")
else ()
  add_custom_command(OUTPUT core/mgmt-ioctl.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_MGMT_IOCTL_H} > core/mgmt-ioctl.rst
    DEPENDS ${XRT_MGMT_IOCTL_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/xocl_ioctl.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XOCL_IOCTL_H} > core/xocl_ioctl.rst
    DEPENDS ${XRT_XOCL_IOCTL_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/zocl_ioctl.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_ZOCL_IOCTL_H} > core/zocl_ioctl.rst
    DEPENDS ${XRT_ZOCL_IOCTL_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_bo.rst
    DEPENDS ${XRT_BO_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_BO_H} > core/xrt_bo.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_device.rst
    DEPENDS ${XRT_DEVICE_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_DEVICE_H} > core/xrt_device.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_ini.rst
    DEPENDS ${XRT_INI_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_INI_H} > core/xrt_ini.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_ip.rst
    DEPENDS ${XRT_IP_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_IP_H} > core/xrt_ip.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_kernel.rst
    DEPENDS ${XRT_KERNEL_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_KERNEL_H} > core/xrt_kernel.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_message.rst
    DEPENDS ${XRT_MESSAGE_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_MESSAGE_H} > core/xrt_message.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_system.rst
    DEPENDS ${XRT_SYSTEM_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_SYSTEM_H} > core/xrt_system.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_uuid.rst
    DEPENDS ${XRT_UUID_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_UUID_H} > core/xrt_uuid.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/xrt_xclbin.rst
    DEPENDS ${XRT_XCLBIN_H}
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XCLBIN_H} > core/xrt_xclbin.rst
    VERBATIM
    )

  add_custom_command(OUTPUT core/ert.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_ERT_H} > core/ert.rst
    DEPENDS ${XRT_ERT_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/mailbox_proto.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_MAILBOX_PROTO_H} > core/mailbox_proto.rst
    DEPENDS ${XRT_MAILBOX_PROTO_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/mailbox.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_MAILBOX_C} > core/mailbox.rst
    DEPENDS ${XRT_MAILBOX_C}
    VERBATIM
    )


  add_custom_target(
    xrt_docs
    DEPENDS
    doxygen
    core/mgmt-ioctl.rst
    core/xocl_ioctl.rst
    core/zocl_ioctl.rst
    core/xrt_bo.rst
    core/xrt_device.rst
    core/xrt_ini.rst
    core/xrt_ip.rst
    core/xrt_kernel.rst
    core/xrt_message.rst
    core/xrt_system.rst
    core/xrt_uuid.rst
    core/xrt_xclbin.rst
    core/ert.rst
    core/mailbox_proto.rst
    core/mailbox.rst
    COMMENT "Generating documentation with Sphinx"
    COMMAND ${CMAKE_COMMAND} -E make_directory  html
    COMMAND rm -rf ${DOC_TOC_DIR}/*
    COMMAND cp -rf ${XRT_DOC_TOC_DIR}/* ${DOC_TOC_DIR}
    COMMAND ${SPHINX_EXECUTABLE} -a ${DOC_TOC_DIR} html
    )
endif ()

# Don't forget to add a rst file under toc/ for html generation
