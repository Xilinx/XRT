# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC}-${CMC_FILENAME})

# =============== CMC GEN2 =====================
# Get and rename the files from the repository
set (CMCGEN2 "CmcGen2")
set (URL_MSP432_GEN2 "https://raw.gitenterprise.xilinx.com/CMC/CMC/U50/release")

# Download the file name, retrieve the file name, and download the FW image with the new file name
FILE(DOWNLOAD ${URL_MSP432_GEN2}/cmc.bin.filename ${CMAKE_CURRENT_BINARY_DIR}/${CMCGEN2}-CMC.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${CMCGEN2}-CMC.filename CMCGEN2_FILENAME)
FILE(DOWNLOAD ${URL_MSP432_GEN2}/cmc.bin ${CMAKE_CURRENT_BINARY_DIR}/${CMCGEN2}-${CMCGEN2_FILENAME} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMCGEN2}-${CMCGEN2_FILENAME})

# =============== CMC NO_SC GEN1 for U2 boards =====================
set (CMC_NO_SC_GEN1 "cmc_no_sc.bin")
set (URL_NO_SC_GEN1 "https://raw.gitenterprise.xilinx.com/CMC/CMC/tree/U2/release")

FILE(DOWNLOAD ${URL_NO_SC_GEN1}/${CMC_NO_SC_GEN1}.filename ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC_GEN1}-CMC.filename STATUS mystatus)
FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC_GEN1}-CMC.filename CMC_NO_SC_GEN1_FILENAME)
FILE(DOWNLOAD ${URL_NO_SC_GEN1}/${CMC_NO_SC_GEN1} ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC_GEN1} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC_GEN1}-${CMC_NO_SC_GEN1_FILENAME})

install (FILES ${CMC_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})
