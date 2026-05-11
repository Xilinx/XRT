# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(XRT_RUNTIME_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(XRT_DOC_TOC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/toc")
set(DOC_CORE_DIR           "${CMAKE_CURRENT_BINARY_DIR}/core")
set(DOC_TOC_DIR            "${CMAKE_CURRENT_BINARY_DIR}/toc")

file(GLOB XRT_BO_H         ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_bo.h)
file(GLOB XRT_DEVICE_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_device.h)
file(GLOB XRT_IP_H         ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ip.h)
file(GLOB XRT_KERNEL_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_kernel.h)
file(GLOB XRT_KERNEL_EXP_H ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_kernel.h)
file(GLOB XRT_MESSAGE_H    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_message.h)
file(GLOB XRT_SYSTEM_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_system.h)
file(GLOB XRT_UUID_H       ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_uuid.h)
file(GLOB XRT_XCLBIN_H     ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_xclbin.h)
file(GLOB XRT_INI_H        ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ini.h)

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
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_kernel.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_elf.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_hw_context.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/xrt_uuid.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ini.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_message.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_ip.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_system.h
    ${XRT_RUNTIME_SRC_DIR}/core/include/xrt/experimental/xrt_xclbin.h
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
    COMMAND echo "PREDEFINED = __cplusplus XCL_DRIVER_DLLESPEC= __cplusplus XRT_API_EXPORT=" >> ${DOXYFILE}
    COMMAND echo "EXPAND_AS_DEFINED = XCL_DRIVER_DLLESPEC XRT_API_EXPORT" >> ${DOXYFILE}
    COMMAND echo "IMAGE_PATH = ${XRT_DOC_TOC_DIR}" >> ${DOXYFILE}
    COMMAND echo "INPUT = ${DOXY_INPUT}" >> ${DOXYFILE}
    COMMAND ${DOXYGEN_EXECUTABLE}
    )
endif(NOT DOXYGEN_EXECUTABLE)

if (NOT KERNELDOC_EXECUTABLE OR NOT SPHINX_EXECUTABLE)
  MESSAGE (WARNING "kernel-doc or Sphinx not found, XRT documentation build disabled")
else ()
  # RST sources present under toc/ but not linked from any toctree cause Sphinx
  # "document isn't included in any toctree" warnings. Remove them from the
  # staging copy before sphinx-build (sources under ${XRT_DOC_TOC_DIR} are unchanged).
  set(XRT_DOC_SPHINX_EXCLUDE_RST
    cloud_vendor_support.rst
    debug-faq.rst
    ert.main.rst
    execution-model.rst
    formats.rst
    fpga_device_ready.rst
    hm.rst
    m2m.rst
    mailbox.main.rst
    mailbox.proto.rst
    mgmt-ioctl.main.rst
    multiprocess.rst
    nagios_plugin.rst
    newxsa-bringup.rst
    opencl_extension.rst
    p2p.rst
    platforms.rst
    platforms_partitions.rst
    security.rst
    sysfs.rst
    test.rst
    vsec.rst
    xball.rst
    xbflash2.rst
    xbmgmt.rst
    xbtools_map.rst
    xbtop.rst
    xocl_ioctl.main.rst
    xrt_include_api_reference.rst
    xrt_api_aie_graph.rst
    xrt_api_buffer.rst
    xrt_api_device_uuid.rst
    xrt_api_experimental_headers.rst
    xrt_api_hw_context.rst
    xrt_api_kernel_run.rst
    xrt_kernel_executions.rst
    yocto.rst
    zocl_ioctl.main.rst
    )
  set(XRT_DOC_SPHINX_EXCLUDE_ABS "")
  foreach(_xrt_doc_rst IN LISTS XRT_DOC_SPHINX_EXCLUDE_RST)
    list(APPEND XRT_DOC_SPHINX_EXCLUDE_ABS "${DOC_TOC_DIR}/${_xrt_doc_rst}")
  endforeach()

  add_custom_target(
    xrt_docs
    DEPENDS
    doxygen
    COMMENT "Generating documentation with Sphinx"
    COMMAND ${CMAKE_COMMAND} -E make_directory  html
    COMMAND rm -rf ${DOC_TOC_DIR}/*
    COMMAND cp -rf ${XRT_DOC_TOC_DIR}/* ${DOC_TOC_DIR}
    COMMAND rm -f ${XRT_DOC_SPHINX_EXCLUDE_ABS}
    COMMAND ${SPHINX_EXECUTABLE} -E ${DOC_TOC_DIR} html
    )
endif ()

# Don't forget to add a rst file under toc/ for html generation
