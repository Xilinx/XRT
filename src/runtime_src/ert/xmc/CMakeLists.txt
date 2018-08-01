add_custom_command(
  OUTPUT xmc.bin
  COMMAND curl https://raw.gitenterprise.xilinx.com/XMC/XMC/XBB-dev/release/xmc.bin --output xmc.bin
  COMMENT "Generating binary for XMC"
)

add_custom_target(xmc
 DEPENDS xmc.bin
)
