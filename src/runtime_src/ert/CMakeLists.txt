if ((EXISTS /tools/batonroot/rodin/devkits) AND (${XRT_NATIVE_BUILD} STREQUAL "yes") AND (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64"))

message("-- Rodin devkits found, preparing ERT build")

set(ERT_INSTALL_PREFIX "/opt/xilinx/xrt/share/fw")

add_subdirectory(scheduler)
add_subdirectory(management)
add_subdirectory(cmc)
add_subdirectory(bmc)

add_custom_target(ert ALL
 DEPENDS scheduler management cmc bmc
)

endif()
