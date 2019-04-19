set (MSP432 "BMC-MSP432")
set (ALVEOGEN1 "AlveoGen1")
set (ALVEOGEN2 "AlveoGen2")
set (ALVEOGEN3 "AlveoGen3")

# Check if BMC repository can be accessed
set (URL_MSP432_GEN1 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master/BMC-card-firmware")
set (URL_MSP432_GEN2 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master-U280/BMC-card-firmware")
set (URL_MSP432_GEN3 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/U280-prod/BMC-card-firmware")
FILE(DOWNLOAD ${URL_MSP432_GEN1}/${MSP432}.txt ./${MSP432}.found.txt STATUS mystatus)
FILE(DOWNLOAD ${URL_MSP432_GEN2}/${MSP432}.txt ./${MSP432}.found.txt STATUS mystatus)
FILE(DOWNLOAD ${URL_MSP432_GEN3}/${MSP432}.txt ./${MSP432}.found.txt STATUS mystatus)
list(GET mystatus 0 error_code)

if (NOT ${error_code})
add_custom_command(
  OUTPUT ${MSP432}.txt {MSP432}.txt.filename

  COMMENT "Alveo Gen1"
  COMMAND mkdir -p ${ALVEOGEN1}
  COMMENT "Downloading ${ALVEOGEN1} MSP432..."
  COMMAND curl ${URL_MSP432_GEN1}/${MSP432}.txt --output ${ALVEOGEN1}/${MSP432}.txt
  COMMAND curl ${URL_MSP432_GEN1}/${MSP432}.txt.filename --output ${ALVEOGEN1}/${MSP432}.txt.filename
  COMMENT "Completed downloading ${ALVEOGEN1} MSP432"
  COMMENT "Rename base file name"
  COMMAND echo -n "Alveo-Gen1:" > ${ALVEOGEN1}/working.txt.filename
  COMMAND cat ${ALVEOGEN1}/${MSP432}.txt.filename >> ${ALVEOGEN1}/working.txt.filename
  COMMAND cat ${ALVEOGEN1}/working.txt.filename | xargs -t mv ${ALVEOGEN1}/${MSP432}.txt

  COMMENT "Alveo Gen2"
  COMMAND mkdir -p ${ALVEOGEN2}
  COMMENT "Downloading ${ALVEOGEN2} MSP432..."
  COMMAND curl ${URL_MSP432_GEN2}/${MSP432}.txt --output ${ALVEOGEN2}/${MSP432}.txt
  COMMAND curl ${URL_MSP432_GEN2}/${MSP432}.txt.filename --output ${ALVEOGEN2}/${MSP432}.txt.filename
  COMMENT "Completed downloading ${ALVEOGEN2} MSP432"
  COMMENT "Rename base file name"
  COMMAND echo -n "Alveo-Gen2:" > ${ALVEOGEN2}/working.txt.filename
  COMMAND cat ${ALVEOGEN2}/${MSP432}.txt.filename >> ${ALVEOGEN2}/working.txt.filename
  COMMAND cat ${ALVEOGEN2}/working.txt.filename | xargs -t mv ${ALVEOGEN2}/${MSP432}.txt

  COMMENT "Alveo Gen3"
  COMMAND mkdir -p ${ALVEOGEN3}
  COMMENT "Downloading ${ALVEOGEN3} MSP432..."
  COMMAND curl ${URL_MSP432_GEN3}/${MSP432}.txt --output ${ALVEOGEN3}/${MSP432}.txt
  COMMAND curl ${URL_MSP432_GEN3}/${MSP432}.txt.filename --output ${ALVEOGEN3}/${MSP432}.txt.filename
  COMMENT "Completed downloading ${ALVEOGEN3} MSP432"
  COMMENT "Rename base file name"
  COMMAND echo -n "Alveo-Gen3:" > ${ALVEOGEN3}/working.txt.filename
  COMMAND cat ${ALVEOGEN3}/${MSP432}.txt.filename >> ${ALVEOGEN3}/working.txt.filename
  COMMAND cat ${ALVEOGEN3}/working.txt.filename | xargs -t mv ${ALVEOGEN3}/${MSP432}.txt
)

add_custom_target(bmc
  DEPENDS ${MSP432}.txt
)

install (
   CODE "file( GLOB _FlashImages \"${CMAKE_CURRENT_BINARY_DIR}/*.txt\" )"
   CODE "file( INSTALL \${_FlashImages} DESTINATION \"${ERT_INSTALL_PREFIX}\" )"
   )
else()
  MESSAGE(WARNING "Skipped MSP432 firmware packaging with error code ${error_code}")
endif()
