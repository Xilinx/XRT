# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
cmake_minimum_required(VERSION 3.5.0)

include(CPackComponent)

# Not support by WIX
#SET(CPACK_SET_DESTDIR ON)

SET(CPACK_GENERATOR WIX)

SET(CPACK_PACKAGE_VENDOR "Xilinx Inc")
SET(CPACK_PACKAGE_CONTACT "sonal.santan@xilinx.com")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Xilinx RunTime stack for use with Xilinx FPGA platforms")

SET(CPACK_PACKAGE_NAME "XRT")
SET(CPACK_PACKAGE_VERSION_RELEASE "${XRT_VERSION_RELEASE}")
SET(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")
SET(CPACK_REL_VER "WindowsServer2019")
SET(CPACK_ARCH "amd64")

SET(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${XRT_VERSION_RELEASE}.${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CPACK_REL_VER}-${CPACK_ARCH}")


message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

# Neet to have an extention
file(COPY "${XRT_SOURCE_DIR}/../LICENSE" DESTINATION "${PROJECT_BINARY_DIR}")
file(RENAME "${PROJECT_BINARY_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/license.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_BINARY_DIR}/license.txt")

# OpenCL is excluded from NPU builds
if (NOT XRT_NPU)

################################################################
# Khronos ICD loader
################################################################
file(GLOB XRT_OPENCL_DLL_LIB_FILES
  "${KHRONOS}/bin/OpenCL.dll"
  )

install(FILES ${XRT_OPENCL_DLL_LIB_FILES}
   DESTINATION xrt/ext/bin
   COMPONENT opencl_libraries)

cpack_add_component(opencl_libraries
  DISPLAY_NAME "OpenCL Libraries"
  DESCRIPTION "OpenCL libraries used by the XRT applications."
  GROUP THIRD_PARTY_LIBRARIES
  ENABLED
)
################################################################
# Label the third party libraries group
################################################################
cpack_add_component_group(THIRD_PARTY_LIBRARIES
  DISPLAY_NAME "Third Party Libraries"
  DESCRIPTION "Third party libraries used by the XRT applications."
)

endif (NOT XRT_NPU)

# xocl/xclmgmt are for Alveo only
if (XRT_ALVEO)
  
# -- XCL Managment Driver --
if (DEFINED XCL_MGMT)
  if (NOT XCL_MGMT STREQUAL "")
    file(GLOB XCL_MGMT_DRIVER
      "${XCL_MGMT}/*"
      )
      install(FILES ${XCL_MGMT_DRIVER}
            DESTINATION xrt/drivers/xcl_mgmt
            COMPONENT xcl_mgmt_driver)

    cpack_add_component(xcl_mgmt_driver
      DISPLAY_NAME "XclMgmt Driver"
      DESCRIPTION "XCL Managment Driver"
      GROUP DRIVERS
      ENABLED
      REQUIRED
    )
  ENDIF(NOT XCL_MGMT STREQUAL "")
ENDIF(DEFINED XCL_MGMT)

# -- XCL Managment2 Driver --
if (DEFINED XCL_MGMT2)
  if (NOT XCL_MGMT2 STREQUAL "")
    file(GLOB XCL_MGMT2_DRIVER
      "${XCL_MGMT2}/*"
      )
      install(FILES ${XCL_MGMT2_DRIVER}
            DESTINATION xrt/drivers/xcl_mgmt2
            COMPONENT xcl_mgmt2_driver)

    cpack_add_component(xcl_mgmt2_driver
      DISPLAY_NAME "XclMgmt2 Driver"
      DESCRIPTION "XCL Managment2 Driver"
      GROUP DRIVERS
      ENABLED
      REQUIRED
    )
  ENDIF(NOT XCL_MGMT2 STREQUAL "")
ENDIF(DEFINED XCL_MGMT2)


# -- Xocl User Driver --
if (DEFINED XOCL_USER)
  if (NOT XOCL_USER STREQUAL "")
    file(GLOB XOCL_USER_DRIVER
      "${XOCL_USER}/*"
      )
      install(FILES ${XOCL_USER_DRIVER}
            DESTINATION xrt/drivers/xocl_user
            COMPONENT xocl_user_driver)

    cpack_add_component(xocl_user_driver
      DISPLAY_NAME "XoclUser Driver"
      DESCRIPTION "XoclUser Driver"
      GROUP DRIVERS
      ENABLED
      REQUIRED
    )
  ENDIF(NOT XOCL_USER STREQUAL "")
ENDIF(DEFINED XOCL_USER)

# -- Xocl User2 Driver --
if (DEFINED XOCL_USER2)
  if (NOT XOCL_USER2 STREQUAL "")
    file(GLOB XOCL_USER2_DRIVER
      "${XOCL_USER2}/*"
      )
      install(FILES ${XOCL_USER2_DRIVER}
            DESTINATION xrt/drivers/xocl_user2
            COMPONENT xocl_user2_driver)

    cpack_add_component(xocl_user2_driver
      DISPLAY_NAME "XoclUser2 Driver"
      DESCRIPTION "XoclUser2 Driver"
      GROUP DRIVERS
      ENABLED
      REQUIRED
    )
  ENDIF(NOT XOCL_USER2 STREQUAL "")
ENDIF(DEFINED XOCL_USER2)


# -- Drivers group --
cpack_add_component_group(DRIVERS
  DISPLAY_NAME "XRT Drivers"
  DESCRIPTION "Drivers used by XRT."
#  EXPANDED
)

endif (XRT_ALVEO)


# -- Our application ---
cpack_add_component(xrt
  DISPLAY_NAME "XRT"
  DESCRIPTION "XRT Tools and supporting libraries."
  REQUIRED
)




# Set the Packaging directory
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "Xilinx")
SET(CPACK_WIX_SKIP_PROGRAM_FOLDER TRUE)
SET(CPACK_WIX_UI_BANNER "${XRT_SOURCE_DIR}/CMake/resources/XilinxBanner.bmp")
SET(CPACK_WIX_UI_DIALOG "${XRT_SOURCE_DIR}/CMake/resources/XilinxDialog.bmp")


add_custom_target(xrtpkg
  echo "COMMAND ${CMAKE_CPACK_COMMAND}"
  COMMAND ${CMAKE_CPACK_COMMAND}
)

include(CPack)
