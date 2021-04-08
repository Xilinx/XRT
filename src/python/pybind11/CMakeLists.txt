find_package(PythonLibs REQUIRED)
find_package(pybind11 REQUIRED PATHS "/usr/local/lib/python${PythonLibs_VERSION_MAJOR}.${PythonLibs_VERSION_MINOR}")

if(PYTHONLIBS_FOUND)
  message("-- Python libs version: ${PYTHONLIBS_VERSION_STRING}")
  message("-- PYTHON_INCLUDE_PATH ${PYTHON_INCLUDE_PATH}")

  INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
  pybind11_add_module(pyxrt src/pyxrt.cpp)
  target_link_libraries(pyxrt PRIVATE xrt_coreutil uuid pthread)

  install(TARGETS pyxrt
    EXPORT xrt-targets
    LIBRARY DESTINATION ${PY_INSTALL_DIR} ${XRT_NAMELINK_SKIP}
    )

else(PYTHONLIBS_FOUND)
  message(WARNING "-- pybind11 or python3 libs not found, pybind11 support disabled --")
endif(PYTHONLIBS_FOUND)
