set(TESTNAME "22_verify")

add_executable(${TESTNAME} main.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY})

# Experimental demo of static linking.
# Should be built into to PUBLIC options along with find_package
if (DEFINED ENV{XRT_STATIC_BUILD})
  add_executable(${TESTNAME}_hw_static main.cpp)
  if (NOT WIN32)
    target_link_options(${TESTNAME}_hw_static PRIVATE "-static" "-L${XRT_LINK_DIRS}")
    target_link_libraries(${TESTNAME}_hw_static
      PRIVATE
      # force linking of whole archive to enable static globals
      -Wl,--whole-archive -lxrt_core_static -Wl,--no-whole-archive
      -Wl,--whole-archive -lrt -lpthread -Wl,--no-whole-archive
      )
  endif()
  target_link_libraries(${TESTNAME}_hw_static
    PRIVATE
    ${xrt_core_static_LIBRARY}
    ${xrt_coreutil_static_LIBRARY}
    )

  install(TARGETS ${TESTNAME}_hw_static
    RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
endif()

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
