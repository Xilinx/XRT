# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.20)

set(CPACK_PACKAGE_VENDOR "Advanced Micro Devices, Inc.")
set(CPACK_PACKAGE_CONTACT "soren.soe@amd.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XDNA RunTime stack for use with AMD FPGA and NPU platforms")

set(CPACK_PACKAGE_NAME "XRT SDK")
set(CPACK_PACKAGE_VERSION_RELEASE "${XRT_VERSION_RELEASE}")
set(CPACK_PACKAGE_VERSION "${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")
set(CPACK_REL_VER "windows")
set(CPACK_ARCH "x86_64")

set(CPACK_ARCHIVE_COMPONENT_INSTALL 1)
set(CPACK_PACKAGE_FILE_NAME "xrtsdk-${CPACK_PACKAGE_VERSION}-${CPACK_REL_VER}-${CPACK_ARCH}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "XRT")
set(CPACK_NSIS_PACKAGE_NAME "XRT SDK ${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MENU_LINKS "")

# Initialize CPACK_COMPONENTS_ALL variable to just the SDK
# component, which is base-dev
set(CPACK_COMPONENTS_ALL "base_dev")
set(CPACK_ARCHIVE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})

message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

include(CPack)

cpack_add_component(base_dev
  DISPLAY_NAME "XRT SDK"
  DESCRIPTION "XRT software development kit with header files and link libraries for application development."
  GROUP Development
  )

cpack_add_component_group(Development
  EXPANDED
  DESCRIPTION "XRT Software Development Kits"
  )
