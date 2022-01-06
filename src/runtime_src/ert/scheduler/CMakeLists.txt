# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# Build ERT under legacy ,u50, v20, and v30 directories

if (${ERT_BUILD_ALL} STREQUAL "yes")

function(GetBSP BSP)
 set(URL https://github.com/Xilinx/ERT-BSP/raw/main/BSPs/${BSP})
 FILE(DOWNLOAD ${URL} ${CMAKE_CURRENT_BINARY_DIR}/${BSP} STATUS dlstatus)
 if(NOT dlstatus EQUAL 0)
  message(FATAL_ERROR "ERROR: Failed to retrieve files from: ${URL}\n"
                       "   error code: ${dlstatus}\n")
 endif()
endfunction()

GetBSP("sched_bsp.tar.bz2")
GetBSP("sched_bsp_u50.tar.bz2")
GetBSP("sched_bsp_v20.tar.bz2")
GetBSP("sched_bsp_v30.tar.bz2")

function(build_ert_fw version)
  string(TOUPPER ${version} CFLAG_VERSION)

  set(BSP_STRING "Extracting ${CFLAG_VERSION} board support packet for ERT scheduler")

  if("${version}" STREQUAL "legacy")
    set(BSP_TAR_NAME "sched_bsp.tar.bz2")
    set(LSCRIPT_NAME "lscript.ld")
    set(SCHE_NAME    "sched.bin")
  else()
    set(BSP_TAR_NAME "sched_bsp_${version}.tar.bz2")
    set(LSCRIPT_NAME "lscript_${version}.ld")
    set(SCHE_NAME    "sched_${version}.bin")
  endif()

  add_custom_command(
   OUTPUT ${version}/bsp.extracted
   COMMAND ${CMAKE_COMMAND} -E make_directory ${version}/bsp
   COMMAND tar -C ${version}/bsp -jxf ${CMAKE_CURRENT_BINARY_DIR}/${BSP_TAR_NAME}
   COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/${LSCRIPT_NAME} ${version}/lscript.ld
   COMMAND touch ${version}/bsp.extracted
   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BSP_TAR_NAME}
   COMMENT ${BSP_STRING}
  )

  add_custom_command(
   OUTPUT ${version}/sched.bin
   COMMAND ${CMAKE_COMMAND} -E make_directory ${version}
   COMMAND $(MAKE) DEFINES="-DERT_BUILD_${CFLAG_VERSION} " BLDDIR=${CMAKE_CURRENT_BINARY_DIR}/${version} SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/sched.mk ert
   DEPENDS ${version}/bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/sched.c
   COMMENT "Generating binary for ERT scheduler"
  )

  install (FILES
   ${CMAKE_CURRENT_BINARY_DIR}/${version}/sched.bin
   DESTINATION ${ERT_INSTALL_PREFIX}
   RENAME ${SCHE_NAME}
   )

  install (FILES
   ${CMAKE_CURRENT_BINARY_DIR}/${version}/sched.bin
   DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
   RENAME ${SCHE_NAME}
   )
endfunction(build_ert_fw)

build_ert_fw(v30)
build_ert_fw(v20)
build_ert_fw(u50)
build_ert_fw(legacy)


add_custom_target(scheduler
 DEPENDS legacy/sched.bin u50/sched.bin v20/sched.bin v30/sched.bin
)

endif(${ERT_BUILD_ALL} STREQUAL "yes")

################################################################
# HW emulation libsched_em
################################################################
file(GLOB SCH_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/sched_hw_emu.c"
  )

SET_SOURCE_FILES_PROPERTIES(${SCH_SRC_FILES} PROPERTIES LANGUAGE CXX )

add_library(sch_objects OBJECT ${SCH_SRC_FILES})

set(CURR_SOURCE "")
add_library(sched_em SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sch_objects>
  )
target_compile_definitions(sch_objects PUBLIC -DERT_HW_EMU -DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)  

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

add_library(sch_objects_v30 OBJECT ${SCH_SRC_FILES})

set(CURR_SOURCE "")
add_library(sched_em_v30 SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:sch_objects_v30>
  )
target_compile_definitions(sch_objects_v30 PUBLIC -DERT_HW_EMU -DERT_BUILD_V30 -DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)

set_target_properties(sched_em_v30 PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

install(TARGETS sched_em_v30
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS sched_em_v30
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
