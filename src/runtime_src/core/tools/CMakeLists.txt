# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if (${XRT_BOOST_VERSION} VERSION_LESS 1.64.0)
  add_compile_options(-DNO_BOOST_PROCESS)
endif()

add_subdirectory(xbutil2)
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(xbmgmt2)
endif()

if(NOT WIN32)
  add_subdirectory(xbflash2)
endif()

install (PROGRAMS "./common/xball" DESTINATION ${XRT_INSTALL_BIN_DIR})
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  if (NOT WIN32)
    # Use PROGRAMS to allow for execution. This is recommended in the CMake documentation
    # https://cmake.org/cmake/help/latest/command/install.html#files
    # This is used for the setup script to enable auto completion
    install (PROGRAMS "./xbmgmt2/xbmgmt-bash-completion" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
    install (PROGRAMS "./xbmgmt2/xbmgmt-csh-completion" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
    install (PROGRAMS "./xbmgmt2/xbmgmt-csh-completion-wrapper" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
    install (PROGRAMS "./xbutil2/xbutil-bash-completion" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
    install (PROGRAMS "./xbutil2/xbutil-csh-completion" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
    install (PROGRAMS "./xbutil2/xbutil-csh-completion-wrapper" DESTINATION "${XRT_INSTALL_DIR}/share/completions")
  endif()
endif()
