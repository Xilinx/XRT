set (XMC "xmc.bin")

add_custom_command(
  OUTPUT xmc.bin
  COMMAND curl https://raw.gitenterprise.xilinx.com/XMC/XMC/XBB-dev/release/${XMC} --output ${XMC}
  COMMENT "Generating binary for XMC"
)

add_custom_target(xmc
 DEPENDS xmc.bin
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/${XMC}
 DESTINATION ${ERT_INSTALL_PREFIX}
)
