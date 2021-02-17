set(TESTNAME "xcl_vcu_test.exe")

set(xrt_core_LIBRARY xrt_core)
set(xrt_coreutil_LIBRARY xrt_coreutil)

if (NOT DEFINED XRT_VALIDATE_DIR)
  set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

add_executable(${TESTNAME} src/plugin_dec.cpp src/xrt_utils.cpp src/main.cpp)
include_directories(../../../src/include/1_2 ../../../src/runtime_src/core/include src/ ../common/includes/xcl2 ../common/includes/cmdparser ../common/includes/logger )
target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})
add_definitions( -Wno-narrowing )

if (NOT WIN32)
  install(TARGETS ${TESTNAME}
    RUNTIME DESTINATION ${XRT_VALIDATE_DIR})
endif(NOT WIN32)

