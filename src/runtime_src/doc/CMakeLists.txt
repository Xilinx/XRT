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

# xma-plg
file(GLOB XRT_XMA_DECODER_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmadecoder.h)
file(GLOB XRT_XMA_ENCODER_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmaencoder.h)
file(GLOB XRT_XMA_FILTER_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmafilter.h)
file(GLOB XRT_XMA_KERNEL_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmakernel.h)
file(GLOB XRT_XMA_SCALER_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmascaler.h)
file(GLOB XRT_XMA_SESS_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/plg/xmasess.h)

 # xma-app
file(GLOB XRT_XMA_BUFFERS_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmabuffers.h)
file(GLOB XRT_XMA_DECODER_APP_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmadecoder.h)
file(GLOB XRT_XMA_ENCODER_APP_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmaencoder.h)
file(GLOB XRT_XMA_ERROR_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmaerror.h)
file(GLOB XRT_XMA_FILTER_APP_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmafilter.h)
file(GLOB XRT_XMA_KERNEL_APP_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmakernel.h)
file(GLOB XRT_XMA_LOGGER_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmalogger.h)
file(GLOB XRT_XMA_PARAM_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmaparam.h)
file(GLOB XRT_XMA_SCALER_APP_H ${XRT_RUNTIME_SRC_DIR}/../xma/include/app/xmascaler.h)

file(MAKE_DIRECTORY ${DOC_CORE_DIR})
file(MAKE_DIRECTORY "${DOC_CORE_DIR}/plg")
file(MAKE_DIRECTORY "${DOC_CORE_DIR}/app")
file(COPY ${XRT_DOC_TOC_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

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

  #xma-app
  add_custom_command(OUTPUT core/app/xmabuffers.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_BUFFERS_H} > core/app/xmabuffers.rst
    DEPENDS ${XRT_XMA_BUFFERS_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmadecoder.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_DECODER_APP_H} > core/app/xmadecoder.rst
    DEPENDS ${XRT_XMA_DECODER_APP_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmaencoder.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_ENCODER_APP_H} > core/app/xmaencoder.rst
    DEPENDS ${XRT_XMA_ENCODER_APP_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmaerror.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_ERROR_H} > core/app/xmaerror.rst
    DEPENDS ${XRT_XMA_ERROR_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmafilter.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_FILTER_APP_H} > core/app/xmafilter.rst
    DEPENDS ${XRT_XMA_FILTER_APP_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmakernel.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_KERNEL_APP_H} > core/app/xmakernel.rst
    DEPENDS ${XRT_XMA_KERNEL_APP_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmalogger.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_LOGGER_H} > core/app/xmalogger.rst
    DEPENDS ${XRT_XMA_LOGGER_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmaparam.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_PARAM_H} > core/app/xmaparam.rst
    DEPENDS ${XRT_XMA_PARAM_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/app/xmascaler.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_SCALER_APP_H} > core/app/xmascaler.rst
    DEPENDS ${XRT_XMA_SCALER_APP_H}
    VERBATIM
    )


   #xma-plg
  add_custom_command(OUTPUT core/plg/xmadecoder.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_DECODER_H} > core/plg/xmadecoder.rst
    DEPENDS ${XRT_XMA_DECODER_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/plg/xmaencoder.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_ENCODER_H} > core/plg/xmaencoder.rst
    DEPENDS ${XRT_XMA_ENCODER_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/plg/xmafilter.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_FILTER_H} > core/plg/xmafilter.rst
    DEPENDS ${XRT_XMA_FILTER_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/plg/xmakernel.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_KERNEL_H} > core/plg/xmakernel.rst
    DEPENDS ${XRT_XMA_KERNEL_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/plg/xmascaler.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_SCALER_H} > core/plg/xmascaler.rst
    DEPENDS ${XRT_XMA_SCALER_H}
    VERBATIM
    )
  add_custom_command(OUTPUT core/plg/xmasess.rst
    COMMAND ${KERNELDOC_EXECUTABLE} -rst ${XRT_XMA_SESS_H} > core/plg/xmasess.rst
    DEPENDS ${XRT_XMA_SESS_H}
    VERBATIM
    )

  add_custom_target(
    xrt_docs
    DEPENDS ${XRT_DOC_SRC} core/mgmt-ioctl.rst core/xocl_ioctl.rst core/xclhal2.rst core/ert.rst core/xma.rst core/xmaplugin.rst core/plg/xmadecoder.rst core/plg/xmaencoder.rst core/plg/xmafilter.rst core/plg/xmakernel.rst core/plg/xmascaler.rst core/plg/xmasess.rst core/app/xmabuffers.rst core/app/xmadecoder.rst core/app/xmaencoder.rst core/app/xmaerror.rst core/app/xmafilter.rst core/app/xmakernel.rst core/app/xmalogger.rst core/app/xmaparam.rst core/app/xmascaler.rst
    COMMENT "Generating documentation with Sphinx"
    COMMAND mkdir -p html
    COMMAND ${SPHINX_EXECUTABLE} ${DOC_TOC_DIR} html
    COMMAND rm -rf ${XRT_RUNTIME_SRC_DIR}/../../docs/
    COMMAND /bin/cp -rf html/ ${XRT_RUNTIME_SRC_DIR}/../../docs/
    COMMAND touch ${XRT_RUNTIME_SRC_DIR}/../../docs/.nojekyll
    VERBATIM
    )
endif ()
