include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/xcl2
  ${XRT_SOURCE_DIR}/include/1_2
  )

pkg_check_modules(XBTEST_GLIB REQUIRED json-glib-1.0 glib-2.0)
include_directories(${XBTEST_GLIB_INCLUDE_DIRS})

add_compile_options("-DPERFORCE_VERSION=0" "-DBUILD_DATE=xyz")
  
file(GLOB XBTEST_SRCS
  "src/*.cpp"
  )

# set(XBTEST_JSON
#   xbtest_pfm_def_u200.json
#   xbtest_pfm_def_u250.json
#   xbtest_pfm_def_u280.json
#   xbtest_pfm_def_u50.json
#   )

# install (FILES ${XBTEST_JSON}
#   DESTINATION ${XRT_INSTALL_DIR}/xbtest
#   COMPONENT xbtest)

add_executable(xbtest ${XBTEST_SRCS})

target_link_libraries(xbtest
  xilinxopencl
  xrt_core
  pthread
  ${XBTEST_GLIB_LIBRARIES}
  )

install(TARGETS xbtest
  RUNTIME
  DESTINATION ${XRT_INSTALL_DIR}/xbtest
  COMPONENT xbtest)

