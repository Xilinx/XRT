set(TESTNAME "03_loopback")

add_executable(03_loopback.exe main.cpp)
target_link_libraries(03_loopback.exe PRIVATE ${xrt_core_LIBRARY} ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(03_loopback.exe PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS 03_loopback.exe
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
