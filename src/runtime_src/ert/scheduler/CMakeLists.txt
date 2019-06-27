add_custom_command(
 OUTPUT bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory bsp
 COMMAND tar -C bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp.tar.bz2
 COMMAND touch bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp.tar.bz2
 COMMENT "Extracting board support packet for ERT scheduler"
)

add_custom_command(
 OUTPUT sched.bin
 COMMAND $(MAKE) BLDDIR=${CMAKE_CURRENT_BINARY_DIR} SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.mk ert
 DEPENDS bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.cpp
 COMMENT "Generating binary for ERT scheduler"
)

add_custom_target(scheduler
 DEPENDS sched.bin
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)
