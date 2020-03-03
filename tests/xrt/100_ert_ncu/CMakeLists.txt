set(EXECNAME "100_ert_ncu")

add_executable(xrt.exe xrt.cpp)
target_link_libraries(xrt.exe PRIVATE ${xrt_core_LIBRARY})

add_executable(xrtx.exe xrtx.cpp)
target_link_libraries(xrtx.exe PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})

add_executable(ocl.exe ocl.cpp)
target_link_libraries(ocl.exe PRIVATE ${xrt_xilinxopencl_LIBRARY})

if (NOT WIN32)
  target_link_libraries(xrt.exe PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtx.exe PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(ocl.exe PRIVATE pthread)
endif(NOT WIN32)

install(TARGETS xrt.exe xrtx.exe ocl.exe
  RUNTIME DESTINATION ${INSTALL_DIR}/${EXECNAME})

add_custom_command(
  OUTPUT kernel.xclbin
  COMMAND ${MAKE} -f xclbin.mk xclbin
  )
  
