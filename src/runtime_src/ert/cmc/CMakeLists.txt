# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC}-${CMC_FILENAME})

# =============== CMC_U2 for U.2 boards =====================

set (CMC_U2 "cmc.bin")
set (URL_CMC_U2 "https://raw.gitenterprise.xilinx.com/rajkumar/CMC/tree/18jul_cs_u2/release")

FILE(DOWNLOAD ${URL_CMC_U2}/${CMC_U2} ${CMAKE_CURRENT_BINARY_DIR}/${CMC_U2} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC_U2}-${CMC_U2_FILENAME})

install (FILES ${CMC_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})
