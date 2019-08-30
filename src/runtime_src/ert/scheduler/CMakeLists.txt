# Build ERT under legacy and u50 directories

add_custom_command(
 OUTPUT legacy/bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory legacy/bsp
 COMMAND tar -C legacy/bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp.tar.bz2
 COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lscript.ld legacy/lscript.ld
 COMMAND touch legacy/bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp.tar.bz2
 COMMENT "Extracting legacy board support packet for ERT scheduler"
)

add_custom_command(
 OUTPUT u50/bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory u50/bsp
 COMMAND tar -C u50/bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_u50.tar.bz2
 COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lscript_u50.ld u50/lscript.ld
 COMMAND touch u50/bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_u50.tar.bz2
 COMMENT "Extracting u50 board support packet for ERT scheduler"
)

add_custom_command(
 OUTPUT legacy/sched.bin
 COMMAND ${CMAKE_COMMAND} -E make_directory legacy
 COMMAND $(MAKE) DEFINES="-DERT_BUILD_LEGACY" BLDDIR=${CMAKE_CURRENT_BINARY_DIR}/legacy SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.mk ert
 DEPENDS legacy/bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.cpp
 COMMENT "Generating binary for ERT scheduler"
)

add_custom_command(
 OUTPUT u50/sched.bin
 COMMAND ${CMAKE_COMMAND} -E make_directory u50
 COMMAND $(MAKE) DEFINES="-DERT_BUILD_U50" BLDDIR=${CMAKE_CURRENT_BINARY_DIR}/u50 SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.mk ert
 DEPENDS u50/bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.cpp
 COMMENT "Generating binary for ERT scheduler"
)

add_custom_target(scheduler
 DEPENDS legacy/sched.bin u50/sched.bin
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/legacy/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/u50/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
 RENAME sched_u50.bin
)
