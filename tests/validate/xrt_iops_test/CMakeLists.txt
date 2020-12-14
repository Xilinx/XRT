set(TESTNAME "xrt_iops_test.exe")

set(xrt_core_LIBRARY xrt_core)
set(xrt_coreutil_LIBRARY xrt_coreutil)

include_directories(../../../src/include/1_2 ../../../src/runtime_src/core/include src/ ../common/includes/xcl2 ../common/includes/cmdparser ../common/includes/logger )
add_executable(${TESTNAME} src/xrt_api_iops.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread ${OpenCL_LIBRARY} ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})
endif(NOT WIN32)

if (NOT DEFINED XRT_VALIDATE_DIR)
    set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

install(TARGETS ${TESTNAME}
    RUNTIME DESTINATION ${XRT_VALIDATE_DIR})
