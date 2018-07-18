# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH
# LINUX_FLAVOR

SET(CPACK_SET_DESTDIR ON)
SET(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")

SET(PACKAGE_KIND "TGZ")
if (${LINUX_FLAVOR} STREQUAL Ubuntu)
  SET(CPACK_GENERATOR "DEB;TGZ")
  SET(PACKAGE_KIND "DEB")
  SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/postinst;${CMAKE_CURRENT_BINARY_DIR}/runtime_src/prerm")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "ocl-icd-opencl-dev (>= 2.2.0), libboost-dev (>=1.58), libboost-filesystem-dev (>=1.58), uuid-dev (>= 2.27.1), dkms (>= 2.2.0), libprotoc-dev (>=2.6.1), protobuf-compiler (>=2.6.1), libncurses5-dev (>=6.0)")
elseif ((${LINUX_FLAVOR} STREQUAL Redhat) OR (${LINUX_FLAVOR} STREQUAL CentOS))
  SET(CPACK_GENERATOR "RPM;TGZ")
  SET(PACKAGE_KIND "RPM")
  SET(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/postinst")
  SET(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/runtime_src/prerm")
  SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local" "/usr/src" "/opt" "/etc/OpenCL" "/etc/OpenCL/vendors" )
  set(CPACK_RPM_PACKAGE_REQUIRES "ocl-icd-devel >= 2.2, boost-devel >= 1.53, boost-filesystem >= 1.53, libuuid-devel >= 2.23.2, dkms >= 2.5.0, protobuf-devel >= 2.5.0, protobuf-compiler >= 2.5.0, ncurses-devel >= 5.9")
else ()
  SET (CPACK_GENERATOR "TGZ")
endif()

message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

SET(CPACK_PACKAGE_NAME "XRT")
SET(CPACK_PACKAGE_VENDOR "Xilinx Inc")
SET(CPACK_PACKAGE_CONTACT "sonal.santan@xilinx.com")

INCLUDE(CPack)
