set(TESTNAME "reset")

add_executable(xcl_reset xcl.cpp)
target_link_libraries(xcl_reset PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY} pthread)

add_executable(xrt_reset xrt.cpp)
target_link_libraries(xrt_reset PRIVATE ${xrt_coreutil_LIBRARY} pthread)

install(TARGETS xcl_reset xrt_reset
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})

