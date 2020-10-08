set(TESTNAME "11_fp_mmult256")

add_executable(${TESTNAME} main.cpp)
target_link_libraries(${TESTNAME} PRIVATE XRT::xrt_core)

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
