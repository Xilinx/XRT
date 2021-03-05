set(TESTNAME "22_verify")

add_executable(${TESTNAME} main.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY})

# Experimental demo of static linking.
# Should be built into to PUBLIC options along with find_package
if (DEFINED ENV{XRT_STATIC_BUILD})
add_executable(${TESTNAME}_hw_static main.cpp)
target_link_options(${TESTNAME}_hw_static PRIVATE "-static")
target_link_libraries(${TESTNAME}_hw_static
  PRIVATE
  -L${XILINX_XRT}/lib
  xrt_core_static
  xrt_coreutil_static
  boost_system
  boost_filesystem
  uuid
  dl
  -Wl,--whole-archive -lxrt_core_static -lrt -lpthread -Wl,--no-whole-archive
  xrt_coreutil_static
  )
install(TARGETS ${TESTNAME}_hw_static
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
endif()

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
