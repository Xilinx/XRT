set(XRT_RUNTIME_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(XRT_DOC_TOC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/toc")
set(DOC_CORE_DIR           "${CMAKE_CURRENT_BINARY_DIR}/core")
set(DOC_TOC_DIR            "${CMAKE_CURRENT_BINARY_DIR}/toc")

file(GLOB XRT_MGMT_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/driver/xclng/include/mgmt-ioctl.h)
file(GLOB XRT_XOCL_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/driver/xclng/include/xocl_ioctl.h)
file(GLOB XRT_XCLHAL2_H    ${XRT_RUNTIME_SRC_DIR}/driver/include/xclhal2.h)
file(GLOB XRT_ERT_H        ${XRT_RUNTIME_SRC_DIR}/driver/include/ert.h)
file(GLOB XRT_XMA_H        ${XRT_RUNTIME_SRC_DIR}/../xma/include/xma.h)
file(GLOB XRT_XMA_PLUGIN_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/xmaplugin.h)

file(MAKE_DIRECTORY ${DOC_CORE_DIR})
file(COPY ${XRT_DOC_TOC_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(KERNELDOC "kernel-doc")
set(KERNELDOC_URL "https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/plain/scripts/kernel-doc?h=v4.14.52")
MESSAGE(STATUS "${KERNELDOC} downloading")
file(DOWNLOAD ${KERNELDOC_URL} "./${KERNELDOC}")
execute_process(COMMAND chmod +x ${KERNELDOC})
find_program(KERNELDOC_EXECUTABLE ${KERNELDOC} PATHS ${CMAKE_BINARY_DIR})

find_program(SPHINX_EXECUTABLE sphinx-build)

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

  add_custom_command(OUTPUT core/xclhal2.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XCLHAL2_H} > core/xclhal2.rst
    DEPENDS ${XRT_XCLHAL2_H}
    VERBATIM
    )

  add_custom_command(OUTPUT core/ert.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_ERT_H} > core/ert.rst
    DEPENDS ${XRT_ERT_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/xma.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_H} > core/xma.rst
    DEPENDS ${XRT_XMA_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/xmaplugin.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_PLUGIN_H} > core/xmaplugin.rst
    DEPENDS ${XRT_XMA_PLUGIN_H}
    VERBATIM
    )

  add_custom_target(
    xrt_docs ALL
    DEPENDS core/mgmt-ioctl.rst core/xocl_ioctl.rst core/xclhal2.rst core/ert.rst core/xma.rst core/xmaplugin.rst
    COMMENT "Generating documentation with Sphinx"
    COMMAND mkdir -p html
    COMMAND rm -rf ${DOC_TOC_DIR}/*
    COMMAND cp -rf ${XRT_DOC_TOC_DIR}/* ${DOC_TOC_DIR}
    COMMAND ${SPHINX_EXECUTABLE} -a ${DOC_TOC_DIR} html
    )
endif ()
