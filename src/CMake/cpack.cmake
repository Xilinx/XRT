# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH
# LINUX_FLAVOR

SET(CPACK_SET_DESTDIR ON)
SET(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")
SET(CPACK_PACKAGE_NAME "xrt")

execute_process(
    COMMAND lsb_release -r -s
    OUTPUT_VARIABLE CPACK_REL_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

SET(PACKAGE_KIND "TGZ")
if (${LINUX_FLAVOR} STREQUAL Ubuntu)
  SET(CPACK_GENERATOR "DEB;TGZ")
  SET(PACKAGE_KIND "DEB")
  SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/postinst;${CMAKE_CURRENT_BINARY_DIR}/prerm")
  SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "OFF")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "ocl-icd-opencl-dev (>= 2.2.0), libboost-dev (>=1.58), libboost-filesystem-dev (>=1.58), uuid-dev (>= 2.27.1), dkms (>= 2.2.0), libprotoc-dev (>=2.6.1), protobuf-compiler (>=2.6.1), libncurses5-dev (>=6.0), lsb-release, libxml2-dev (>=2.9.1), libyaml-dev (>= 0.1.6)")
elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS)")
  SET(CPACK_GENERATOR "RPM;TGZ")
  SET(PACKAGE_KIND "RPM")
  SET(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/postinst")
  SET(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/prerm")
  SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local" "/usr/src" "/opt" "/etc/OpenCL" "/etc/OpenCL/vendors" "/usr/lib" "/usr/lib/pkgconfig")
  set(CPACK_RPM_PACKAGE_REQUIRES "ocl-icd-devel >= 2.2, boost-devel >= 1.53, boost-filesystem >= 1.53, libuuid-devel >= 2.23.2, dkms >= 2.5.0, protobuf-devel >= 2.5.0, protobuf-compiler >= 2.5.0, ncurses-devel >= 5.9, redhat-lsb-core, libxml2-devel >= 2.9.1, libyaml-devel >= 0.1.4 ")
else ()
  SET (CPACK_GENERATOR "TGZ")
endif()

SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CPACK_REL_VER}")

message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

SET(CPACK_PACKAGE_VENDOR "Xilinx Inc")
SET(CPACK_PACKAGE_CONTACT "sonal.santan@xilinx.com")

INCLUDE(CPack)
