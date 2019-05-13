# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC} DESTINATION ${ERT_INSTALL_PREFIX})