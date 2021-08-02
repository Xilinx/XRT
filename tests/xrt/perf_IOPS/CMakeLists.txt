set(TESTNAME "perf_IOPS")

add_executable(xrt_api_iops xrt_api_iops.cpp)
target_link_libraries(xrt_api_iops PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(xcl_api_iops xcl_api_iops.cpp)
target_link_libraries(xcl_api_iops  PRIVATE ${xrt_coreutil_LIBRARY})
target_link_libraries(xcl_api_iops  PRIVATE ${xrt_core_LIBRARY})

if (NOT WIN32)
  target_link_libraries(xrt_api_iops PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xcl_api_iops PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)


install(FILES xrt.ini DESTINATION ${INSTALL_DIR}/${TESTNAME})

install(TARGETS xrt_api_iops xcl_api_iops
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
