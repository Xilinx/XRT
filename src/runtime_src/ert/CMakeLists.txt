if (EXISTS /tools/batonroot/rodin/devkits)

message("-- Rodin devkits found, preparing ERT build")

add_subdirectory(scheduler)
add_subdirectory(management)

add_custom_target(ert ALL
 DEPENDS scheduler management
)

set(ERT_INSTALL_PREFIX "/opt/xilinx/xrt/share/fw")

install (FILES  
 ${CMAKE_CURRENT_BINARY_DIR}/scheduler/sched.bin
 ${CMAKE_CURRENT_BINARY_DIR}/management/mgmt.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)

endif()


