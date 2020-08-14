# Build ERT under legacy and u50 directories

if (${ERT_BUILD_ALL} STREQUAL "yes")

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
 OUTPUT v20/bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory v20/bsp
 COMMAND tar -C v20/bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_v20.tar.bz2
 COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lscript_v20.ld v20/lscript.ld
 COMMAND touch v20/bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_v20.tar.bz2
 COMMENT "Extracting v2.0 board support packet for ERT scheduler"
)

add_custom_command(
 OUTPUT v30/bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory v30/bsp
 COMMAND tar -C v30/bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_v30.tar.bz2
 COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lscript_v30.ld v30/lscript.ld
 COMMAND touch v30/bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sched_bsp_v30.tar.bz2
 COMMENT "Extracting v3.0 board support packet for ERT scheduler"
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

add_custom_command(
 OUTPUT v20/sched.bin
 COMMAND ${CMAKE_COMMAND} -E make_directory v20
 COMMAND $(MAKE) DEFINES="-DERT_BUILD_V20" BLDDIR=${CMAKE_CURRENT_BINARY_DIR}/v20 SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.mk ert
 DEPENDS v20/bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.cpp
 COMMENT "Generating binary for ERT scheduler"
)

add_custom_command(
 OUTPUT v30/sched.bin
 COMMAND ${CMAKE_COMMAND} -E make_directory v30
 COMMAND $(MAKE) DEFINES="-DERT_BUILD_V30" BLDDIR=${CMAKE_CURRENT_BINARY_DIR}/v30 SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.mk ert
 DEPENDS v30/bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.cpp
 COMMENT "Generating binary for ERT scheduler"
)

add_custom_target(scheduler
 DEPENDS legacy/sched.bin u50/sched.bin v20/sched.bin v30/sched.bin
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/legacy/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/legacy/sched.bin
 DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
  )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/u50/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
 RENAME sched_u50.bin
 )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/u50/sched.bin
 DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
 RENAME sched_u50.bin
  )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/v20/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
 RENAME sched_v20.bin
 )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/v20/sched.bin
 DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
 RENAME sched_v20.bin
 )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/v30/sched.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
 RENAME sched_v30.bin
 )

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/v30/sched.bin
 DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
 RENAME sched_v30.bin
 )

endif(${ERT_BUILD_ALL} STREQUAL "yes")

################################################################
# HW emulation libsched_em
################################################################
file(GLOB SCH_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

add_definitions(-DERT_HW_EMU -DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(sch_objects OBJECT ${SCH_SRC_FILES})

set(CURR_SOURCE "")
add_library(sched_em SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sch_objects>
  )

set_target_properties(sched_em PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

install(TARGETS sched_em
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS sched_em
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
