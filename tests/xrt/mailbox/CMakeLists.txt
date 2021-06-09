set(TESTNAME "mailbox")

add_executable(use_mailbox use_mailbox.cpp)
target_link_libraries(use_mailbox PRIVATE ${xrt_coreutil_LIBRARY})
add_executable(no_mailbox no_mailbox.cpp)
target_link_libraries(no_mailbox PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(use_mailbox PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(no_mailbox PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS use_mailbox no_mailbox
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
