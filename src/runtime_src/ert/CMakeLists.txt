set(ERT_INSTALL_PREFIX "/opt/xilinx/xrt/share/fw")
set(ERT_INSTALL_FIRMWARE_PREFIX "/lib/firmware/xilinx")

if ((EXISTS $ENV{XILINX_VITIS}/gnu/microblaze) AND (${XRT_NATIVE_BUILD} STREQUAL "yes") AND (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64"))

message("-- MicroBlaze toolchain found, preparing ERT build")

add_subdirectory(scheduler)
add_subdirectory(management)
add_subdirectory(cmc)
add_subdirectory(bmc)

add_custom_target(ert ALL
 DEPENDS scheduler management cmc bmc
 )

elseif (DEFINED ENV{XRT_FIRMWARE_DIR})

file(GLOB XRTFW_FILES
  "$ENV{XRT_FIRMWARE_DIR}/*"
  )

message("-- Firmare files copied from ${XRTFW_FILES}")

file(GLOB ERTFW_FILES
  "$ENV{XRT_FIRMWARE_DIR}/sched*.bin"
  )

message("-- ERT firmare files copied from ${ERTFW_FILES}")

install(FILES
  ${XRTFW_FILES}
  DESTINATION ${ERT_INSTALL_PREFIX}
  )

install(FILES
  ${ERTFW_FILES}
  DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
  )

else()

message(WARNING
"****************************************************************\n\
No firmware files built or copied, resulting XRT package will be missing ERT \
scheduler firmware.  Use build.sh -ertfw <dir> to specify path to a directory \
with firmware to copy during XRT build.\n
****************************************************************")

endif()
