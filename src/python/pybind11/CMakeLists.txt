# SPDX-License-Identifier: Apache-2.0

find_package(PythonLibs 3.4.0)
if (PythonLibs_FOUND)
  message("-- Python libs version: ${PYTHONLIBS_VERSION_STRING}")
  message("-- PYTHON_INCLUDE_PATH ${PYTHON_INCLUDE_PATH}")
  string(REPLACE "." ";" PYTHONLIBS_VERSION_LIST ${PYTHONLIBS_VERSION_STRING})
  list(GET PYTHONLIBS_VERSION_LIST 0 PYTHONLIBS_VERSION_MAJOR)
  list(GET PYTHONLIBS_VERSION_LIST 1 PYTHONLIBS_VERSION_MINOR)

  if (${LINUX_FLAVOR} MATCHES "^(Ubuntu|Debian)")
    SET(PKGDIR "dist-packages")
  elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS|Amazon|Fedora|SUSE)")
    SET(PKGDIR "site-packages")
  endif(${LINUX_FLAVOR} MATCHES "^(Ubuntu|Debian)")

  find_package(pybind11 2.6.0 PATHS "/usr/local/lib/python${PYTHONLIBS_VERSION_MAJOR}.${PYTHONLIBS_VERSION_MINOR}/${PKGDIR}/pybind11")
endif(PythonLibs_FOUND)

if (pybind11_FOUND)
  INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
  pybind11_add_module(pyxrt src/pyxrt.cpp)
  target_link_libraries(pyxrt PRIVATE xrt_coreutil uuid pthread)

  install(TARGETS pyxrt
    EXPORT xrt-targets
    LIBRARY DESTINATION ${PY_INSTALL_DIR} ${XRT_NAMELINK_SKIP}
    )

else(pybind11_FOUND)
  message(WARNING "-- pybind11 or python3 libs not found, pybind11 support disabled")
endif(pybind11_FOUND)
