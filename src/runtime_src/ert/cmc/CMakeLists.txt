# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC})

# =============== CMC =====================
# Now get U50 version of CMC...
set (CMC_U50 "cmc_u50.bin")
set (URL_MSP432_U50 "https://raw.gitenterprise.xilinx.com/CMC/CMC/U50/release")

FILE(DOWNLOAD ${URL_MSP432_U50}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC_U50} STATUS mystatus)

list(APPEND CMC_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC_U50})

install (FILES ${CMC_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})