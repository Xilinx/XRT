set(TESTNAME "100_ert_ncu")

add_executable(xrt xrt.cpp)
target_link_libraries(xrt PRIVATE ${xrt_core_LIBRARY})

add_executable(xrtx xrtx.cpp)
target_link_libraries(xrtx PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})

add_executable(xrtxx xrtxx.cpp)
target_link_libraries(xrtxx PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})

add_executable(ocl ocl.cpp)
target_link_libraries(ocl PRIVATE ${xrt_xilinxopencl_LIBRARY})
if (WIN32)
  set(OCL_ROOT c:/Xilinx/XRT/ext)
  set(OpenCL_INCLUDE_DIR ${OCL_ROOT}/include)
  find_library(OpenCL_LIBRARY
    NAMES OpenCL
    HINTS "${OCL_ROOT}/lib")

  target_include_directories(ocl PUBLIC ${OpenCL_INCLUDE_DIR})
endif (WIN32)
target_compile_options(ocl PUBLIC
  "-DCL_TARGET_OPENCL_VERSION=120"
  "-DCL_HPP_MINIMUM_OPENCL_VERSION=120"
  "-DCL_HPP_TARGET_OPENCL_VERSION=120"
  "-DCL_USE_DEPRECATED_OPENCL_1_2_APIS"
  )

if (NOT WIN32)
  target_link_libraries(xrt PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtx PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtxx PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(ocl PRIVATE pthread)
endif(NOT WIN32)

install(TARGETS xrt xrtx xrtxx ocl
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})

add_custom_command(
  OUTPUT kernel.xclbin
  COMMAND ${MAKE} -f xclbin.mk xclbin
  )
