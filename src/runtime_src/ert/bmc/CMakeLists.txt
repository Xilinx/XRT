# =============== ALVEO GEN1 =====================
# Get and rename the files from the repository
set (ALVEOGEN1 "AlveoGen1")
set (URL_MSP432_GEN1 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master/BMC-card-firmware")

# Download the file name, retrieve the file name, and download the FW image with the new file name
FILE(DOWNLOAD ${URL_MSP432_GEN1}/BMC-MSP432.txt.filename ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN1}-BMC-MSP432.txt.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN1}-BMC-MSP432.txt.filename ALVEOGEN1_FILENAME)
FILE(DOWNLOAD ${URL_MSP432_GEN1}/BMC-MSP432.txt ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN1}-${ALVEOGEN1_FILENAME} STATUS mystatus)

list(APPEND ALVEO_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN1}-${ALVEOGEN1_FILENAME})

# =============== ALVEO GEN2 =====================
# Get and rename the files from the repository
set (ALVEOGEN2 "AlveoGen2")
set (URL_MSP432_GEN2 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master-U280/BMC-card-firmware")

# Download the file name, retrieve the file name, and download the FW image with the new file name
FILE(DOWNLOAD ${URL_MSP432_GEN2}/BMC-MSP432.txt.filename ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN2}-BMC-MSP432.txt.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN2}-BMC-MSP432.txt.filename ALVEOGEN2_FILENAME)
FILE(DOWNLOAD ${URL_MSP432_GEN2}/BMC-MSP432.txt ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN2}-${ALVEOGEN2_FILENAME} STATUS mystatus)

list(APPEND ALVEO_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN2}-${ALVEOGEN2_FILENAME})

# =============== ALVEO GEN3 =====================
# Get and rename the files from the repository
set (ALVEOGEN3 "AlveoGen3")
set (URL_MSP432_GEN3 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/U280-prod/BMC-card-firmware")

# Download the file name, retrieve the file name, and download the FW image with the new file name
FILE(DOWNLOAD ${URL_MSP432_GEN3}/BMC-MSP432.txt.filename ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN3}-BMC-MSP432.txt.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN3}-BMC-MSP432.txt.filename ALVEOGEN3_FILENAME)
FILE(DOWNLOAD ${URL_MSP432_GEN3}/BMC-MSP432.txt ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN3}-${ALVEOGEN3_FILENAME} STATUS mystatus)

list(APPEND ALVEO_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN3}-${ALVEOGEN3_FILENAME})

# =============== ALVEO GEN4 =====================
# Get and rename the files from the repository
set (ALVEOGEN4 "AlveoGen4")
set (URL_MSP432_GEN4 "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/U50/BMC-card-firmware")

# Download the file name, retrieve the file name, and download the FW image with the new file name
FILE(DOWNLOAD ${URL_MSP432_GEN4}/BMC-MSP432.txt.filename ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN4}-BMC-MSP432.txt.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN4}-BMC-MSP432.txt.filename ALVEOGEN4_FILENAME)
FILE(DOWNLOAD ${URL_MSP432_GEN4}/BMC-MSP432.txt ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN4}-${ALVEOGEN4_FILENAME} STATUS mystatus)

list(APPEND ALVEO_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN4}-${ALVEOGEN4_FILENAME})

install (FILES ${ALVEO_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})
