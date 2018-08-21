set (MSP432 "MSP432P401R-VCU1525-1.4-56027182079c0bd621761b7dab5a27ca.txt")

add_custom_command(
  OUTPUT msp432
  COMMAND curl https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master/BMC-card-firmware/${MSP432} --output ${MSP432}
  COMMENT "Downloading MSP432"
)

add_custom_target(bmc
 DEPENDS msp432
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/${MSP432}
 DESTINATION ${ERT_INSTALL_PREFIX}
)
