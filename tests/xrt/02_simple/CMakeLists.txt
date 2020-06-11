set(TESTNAME "02_simple")

add_executable(02_simple.exe main.cpp)
target_link_libraries(02_simple.exe PRIVATE ${xrt_core_LIBRARY} ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(02_simple.exe PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS 02_simple.exe
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
