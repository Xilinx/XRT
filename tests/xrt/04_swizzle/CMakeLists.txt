set(TESTNAME "04_swizzle")

add_executable(04_swizzle main.cpp)
target_link_libraries(04_swizzle PRIVATE ${xrt_core_LIBRARY} ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(04_swizzle PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS 04_swizzle
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
