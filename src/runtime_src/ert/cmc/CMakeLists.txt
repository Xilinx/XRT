# =============== CMC =====================

set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${CMC} ${CMAKE_CURRENT_BINARY_DIR}/${CMC} STATUS mystatus)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMC} DESTINATION ${ERT_INSTALL_PREFIX})


# =============== XMC =====================

set (XMC "xmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/XMC/XMC/master/release")

FILE(DOWNLOAD ${URL_MSP432}/${XMC} ${CMAKE_CURRENT_BINARY_DIR}/${XMC} STATUS mystatus)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${XMC} DESTINATION ${ERT_INSTALL_PREFIX})


#set (CMC "cmc.bin")
#set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")
#
## Check if XMC repository can be accessed
#FILE(DOWNLOAD ${URL_MSP432}/${CMC} ./${CMC}.found.txt STATUS mystatus)
#list(GET mystatus 0 error_code)
#
#if (NOT ${error_code})
#add_custom_command(
#  OUTPUT ${CMC}
#  COMMAND curl https://raw.gitenterprise.xilinx.com/XMC/XMC/XBB-dev/release/${CMC} --output ${CMC}
#  COMMENT "Generating binary for XMC"
#)
#
#add_custom_target(cmc
# DEPENDS ${CMC}
#)
#
#install (FILES
# ${CMAKE_CURRENT_BINARY_DIR}/${CMC}
# DESTINATION ${ERT_INSTALL_PREFIX}
# )
#else()
#  MESSAGE(WARNING "Skipped ${XMC} packaging with error code ${error_code}")
#endif()
