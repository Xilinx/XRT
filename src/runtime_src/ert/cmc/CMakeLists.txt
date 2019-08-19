# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC}-${CMC_FILENAME})

# =============== CMC_NO_SC for U.2 boards =====================

set (CMC_NO_SC "cmc_no_sc.bin")
set (URL_CMC_NO_SC "https://raw.gitenterprise.xilinx.com/rajkumar/CMC/tree/18jul_cs_u2/release")

FILE(DOWNLOAD ${URL_CMC_NO_SC}/${CMC_NO_SC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC_NO_SC}-${CMC_NO_SC_FILENAME})

install (FILES ${CMC_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})
