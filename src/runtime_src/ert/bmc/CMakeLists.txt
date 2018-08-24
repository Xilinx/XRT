set (MSP432 "BMC-MSP432")

add_custom_command(
  OUTPUT ${MSP432}.txt ${MSP432}.txt.md5 ${MSP432}.txt.version
  COMMAND curl https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/rel-v1.5/BMC-card-firmware/${MSP432}.txt --output ${MSP432}.txt
  COMMAND curl https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/rel-v1.5/BMC-card-firmware/${MSP432}.txt.md5 --output ${MSP432}.txt.md5
  COMMAND curl https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/rel-v1.5/BMC-card-firmware/${MSP432}.txt.version --output ${MSP432}.txt.version
  COMMENT "Downloading MSP432"
)

add_custom_target(bmc
 DEPENDS ${MSP432}.txt ${MSP432}.txt.md5 ${MSP432}.txt.version
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/${MSP432}.txt
 ${CMAKE_CURRENT_BINARY_DIR}/${MSP432}.txt.md5
 ${CMAKE_CURRENT_BINARY_DIR}/${MSP432}.txt.version
 DESTINATION ${ERT_INSTALL_PREFIX}
)
