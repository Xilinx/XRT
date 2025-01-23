# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if (NOT WIN32)
  set(XRT_SETUP_SCRIPTS
    setup.sh
    setup.csh)

  set(XRT_LOADER_SCRIPTS
    loader)

  set (XRT_SCRIPTS
    service_bundle.sh
    plp_program.sh)

else()

  set(XRT_SETUP_SCRIPTS
    setup.bat)

  set (XRT_LOADER_SCRIPTS
    loader.bat)

endif(NOT WIN32)

if (XRT_BASE)
  install (PROGRAMS ${XRT_LOADER_SCRIPTS}
    DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR}
    COMPONENT ${XRT_BASE_COMPONENT})

  install (FILES ${XRT_SETUP_SCRIPTS}
    DESTINATION ${XRT_INSTALL_DIR}
    COMPONENT ${XRT_BASE_COMPONENT})
endif()

if (XRT_ALVEO)
  install (PROGRAMS ${XRT_SCRIPTS}
    DESTINATION ${XRT_INSTALL_BIN_DIR}
    COMPONENT ${XRT_COMPONENT})
endif()
