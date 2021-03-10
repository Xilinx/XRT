if (${XRT_BOOST_VERSION} VERSION_LESS 1.64.0)
  add_compile_options(-DNO_BOOST_PROCESS)
endif()

add_subdirectory(xbutil2)
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(xbmgmt2)
endif()
