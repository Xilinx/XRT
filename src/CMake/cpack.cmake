# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH
# LINUX_FLAVOR

SET(CPACK_SET_DESTDIR ON)
SET(CPACK_PACKAGE_VERSION_RELEASE "${XRT_VERSION_RELEASE}")
SET(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")
SET(CPACK_PACKAGE_NAME "xrt")

SET(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
SET(CPACK_DEB_COMPONENT_INSTALL ON)
SET(CPACK_RPM_COMPONENT_INSTALL ON)

execute_process(
    COMMAND lsb_release -r -s
    OUTPUT_VARIABLE CPACK_REL_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include (CMake/glibc.cmake)

# Trick to get the Boost Version string and one version greater
SET(Boost_MINOR_VERSION_ONEGREATER "${Boost_MINOR_VERSION}")
MATH(EXPR Boost_MINOR_VERSION_ONEGREATER "1 + ${Boost_MINOR_VERSION}")
SET(Boost_VER_STR "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}")
SET(Boost_VER_STR_ONEGREATER "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION_ONEGREATER}")

SET(PACKAGE_KIND "TGZ")
if (${LINUX_FLAVOR} MATCHES "^(Ubuntu|Debian|pynqlinux)")
  SET(CPACK_GENERATOR "DEB;TGZ")
  SET(PACKAGE_KIND "DEB")
  # Modify the package name for the xrt component
  # Syntax is set(CPACK_<GENERATOR>_<COMPONENT>_PACKAGE_NAME "<name">)
  SET(CPACK_DEBIAN_XRT_PACKAGE_NAME "xrt")

  SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/postinst;${CMAKE_CURRENT_BINARY_DIR}/prerm")
  SET(CPACK_DEBIAN_AWS_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/aws/postinst;${CMAKE_CURRENT_BINARY_DIR}/aws/prerm")
  SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "OFF")
  # aws component dependencies
  SET(CPACK_DEBIAN_AWS_PACKAGE_DEPENDS "xrt (>= ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH})")
  # xrt component dependencies
  SET(CPACK_DEBIAN_XRT_PACKAGE_DEPENDS "ocl-icd-opencl-dev (>= 2.2.0), libboost-dev (>= ${Boost_VER_STR}), libboost-dev (<< ${Boost_VER_STR_ONEGREATER}), libboost-filesystem-dev (>=${Boost_VER_STR}), libboost-filesystem-dev (<<${Boost_VER_STR_ONEGREATER}), uuid-dev (>= 2.27.1), dkms (>= 2.2.0), libprotoc-dev (>=2.6.1), libssl-dev (>=1.0.2), protobuf-compiler (>=2.6.1), libncurses5-dev (>=6.0), lsb-release, libxml2-dev (>=2.9.1), libyaml-dev (>= 0.1.6), libc6 (>= ${GLIBC_VERSION}), libc6 (<< ${GLIBC_VERSION_ONEGREATER}), python (>= 2.7)")

elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS)")
  SET(CPACK_GENERATOR "RPM;TGZ")
  SET(PACKAGE_KIND "RPM")
  # Modify the package name for the xrt component
  # Syntax is set(CPACK_<GENERATOR>_<COMPONENT>_PACKAGE_NAME "<name">)
  set(CPACK_RPM_XRT_PACKAGE_NAME "xrt")

  SET(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/postinst")
  SET(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/prerm")
  SET(CPACK_RPM_AWS_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/aws/postinst")
  SET(CPACK_RPM_AWS_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/aws/prerm")
  SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local" "/usr/src" "/opt" "/etc/OpenCL" "/etc/OpenCL/vendors" "/usr/lib" "/usr/lib/pkgconfig" "/usr/lib64/pkgconfig")
  # aws component dependencies
  SET(CPACK_RPM_AWS_PACKAGE_REQUIRES "xrt >= ${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH}")
  # xrt component dependencies
  SET(CPACK_RPM_XRT_PACKAGE_REQUIRES "ocl-icd-devel >= 2.2, boost-devel >= 1.53, boost-filesystem >= 1.53, libuuid-devel >= 2.23.2, dkms >= 2.5.0, protobuf-devel >= 2.5.0, protobuf-compiler >= 2.5.0, ncurses-devel >= 5.9, redhat-lsb-core, libxml2-devel >= 2.9.1, libyaml-devel >= 0.1.4, python >= 2.7, openssl-devel >= 1.0.2 ")
else ()
  SET (CPACK_GENERATOR "TGZ")
endif()

SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${XRT_VERSION_RELEASE}.${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CPACK_REL_VER}")

message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

SET(CPACK_PACKAGE_VENDOR "Xilinx Inc")
SET(CPACK_PACKAGE_CONTACT "sonal.santan@xilinx.com")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Xilinx RunTime stack for use with Xilinx FPGA platforms")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")

add_custom_target(xrtpkg
  echo "COMMAND ${CMAKE_CPACK_COMMAND}"
  COMMAND ${CMAKE_CPACK_COMMAND}
  COMMAND -mv -f ${CPACK_PACKAGE_FILE_NAME}-xrt.deb ${CPACK_PACKAGE_FILE_NAME}.deb 2> /dev/null
  COMMAND -mv -f ${CPACK_PACKAGE_FILE_NAME}-xrt.rpm ${CPACK_PACKAGE_FILE_NAME}.rpm 2> /dev/null
  COMMAND -mv -f ${CPACK_PACKAGE_FILE_NAME}-xrt.tar.gz ${CPACK_PACKAGE_FILE_NAME}.tar.gz 2> /dev/null

)

INCLUDE(CPack)
