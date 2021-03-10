set(TESTNAME "xrt_iops_test.exe")

set(xrt_core_LIBRARY xrt_core)
set(xrt_coreutil_LIBRARY xrt_coreutil)

if (NOT DEFINED XRT_VALIDATE_DIR)
    set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

if (NOT WIN32)
  include_directories(../common/includes/cmdparser ../common/includes/logger)
  add_executable(${TESTNAME} ../common/includes/cmdparser/cmdlineparser.cpp ../common/includes/logger/logger.cpp src/xrt_api_iops.cpp)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})
  install(TARGETS ${TESTNAME}
    RUNTIME DESTINATION ${XRT_VALIDATE_DIR})
endif(NOT WIN32)

