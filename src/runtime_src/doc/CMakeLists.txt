set(XRT_RUNTIME_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(XRT_DOC_TOC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/toc")
set(DOC_CORE_DIR           "${CMAKE_CURRENT_BINARY_DIR}/core")
set(DOC_TOC_DIR            "${CMAKE_CURRENT_BINARY_DIR}/toc")

file(GLOB XRT_MGMT_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/driver/xclng/include/mgmt-ioctl.h)
file(GLOB XRT_XOCL_IOCTL_H ${XRT_RUNTIME_SRC_DIR}/driver/xclng/include/xocl_ioctl.h)
file(GLOB XRT_XCLHAL2_H    ${XRT_RUNTIME_SRC_DIR}/driver/include/xclhal2.h)
file(GLOB XRT_ERT_H        ${XRT_RUNTIME_SRC_DIR}/driver/include/ert.h)

execute_process(COMMAND mkdir -p ${DOC_CORE_DIR})
execute_process(COMMAND /bin/cp -rf ${XRT_DOC_TOC_DIR} ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB XRT_DOC_SRC
  ${DOC_TOC_DIR}/*.rst
  )

set(KERNELDOC "./kernel-doc")
set(KERNELDOC_URL "https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/plain/scripts/kernel-doc?h=v4.14.52")
MESSAGE(STATUS "${KERNELDOC} downloading")
file(DOWNLOAD ${KERNELDOC_URL} ${KERNELDOC})
execute_process(COMMAND chmod +x ${KERNELDOC})
find_program(KERNELDOC_EXECUTABLE ${KERNELDOC} PATHS "./")

find_program(SPHINX_EXECUTABLE sphinx-build)

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

set(HTML_FILES "html/*")

add_custom_target(
  xrt_doc ALL
  DEPENDS ${XRT_DOC_SRC} core/mgmt-ioctl.rst core/xocl_ioctl.rst core/xclhal2.rst core/ert.rst
  COMMENT "Generating documentation with Sphinx"
  COMMAND mkdir -p html
  COMMAND ${SPHINX_EXECUTABLE} ${DOC_TOC_DIR} html
  COMMAND rm -rf ${XRT_RUNTIME_SRC_DIR}/../../docs/
  COMMAND /bin/cp -rf html/ ${XRT_RUNTIME_SRC_DIR}/../../docs/
  COMMAND touch ${XRT_RUNTIME_SRC_DIR}/../../docs/.nojekyll
  VERBATIM
  )
