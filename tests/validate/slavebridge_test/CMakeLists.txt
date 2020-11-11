set(TESTNAME "slavebridge.exe")

if (WIN32)
  set(OpenCL_INCLUDE_DIR ${OCL_ROOT}/include)
  find_library(OpenCL_LIBRARY
    NAMES OpenCL
    HINTS "${OCL_ROOT}/lib")
  include_directories(${OpenCL_INCLUDE_DIR})
else()
  find_package(OpenCL)
endif(WIN32)

include_directories(../../../src/include/1_2 ../../../src/runtime_src/core/include src/ ../common/includes/xcl2 ../common/includes/cmdparser ../common/includes/logger )
add_executable(${TESTNAME} ../common/includes/xcl2/xcl2.cpp ../common/includes/cmdparser/cmdlineparser.cpp ../common/includes/logger/logger.cpp src/host.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread ${OpenCL_LIBRARY} )
endif(NOT WIN32)

if (NOT DEFINED XRT_VALIDATE_DIR)
    set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${XRT_VALIDATE_DIR})
