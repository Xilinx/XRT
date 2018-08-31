set (MSP432 "BMC-MSP432")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master/BMC-card-firmware")

add_custom_command(
  OUTPUT ${MSP432}.txt {MSP432}.txt.filename 

  COMMENT "Downloading MSP432..."
  COMMAND curl ${URL_MSP432}/${MSP432}.txt --output ${MSP432}.txt
  COMMAND curl ${URL_MSP432}/${MSP432}.txt.filename --output ${MSP432}.txt.filename
  COMMENT "Completed downloading MSP432"

  COMMENT "Rename base file name"
  COMMAND cat BMC-MSP432.txt.filename | tr -d ' ' | xargs -d '\\n'  -t -I % mv BMC-MSP432.txt %.txt
)

add_custom_target(bmc
  DEPENDS ${MSP432}.txt
)

install (
   CODE "file( GLOB _FlashImages \"${CMAKE_CURRENT_BINARY_DIR}/*.txt\" )"
   CODE "file( INSTALL \${_FlashImages} DESTINATION \"${ERT_INSTALL_PREFIX}\" )"
)
