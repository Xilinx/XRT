if ((EXISTS /tools/batonroot/rodin/devkits) AND (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL x86_64))

message("-- Rodin devkits found, preparing ERT build")

add_subdirectory(scheduler)
add_subdirectory(management)
add_subdirectory(xmc)

add_custom_target(ert ALL
 DEPENDS scheduler management xmc
)

set(ERT_INSTALL_PREFIX "/opt/xilinx/xrt/share/fw")

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/scheduler/sched.bin
 ${CMAKE_CURRENT_BINARY_DIR}/management/mgmt.bin
 ${CMAKE_CURRENT_BINARY_DIR}/xmc/xmc.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)

endif()
