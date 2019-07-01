set (XMC "xmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/XMC/XMC/XBB-dev/release")

# Check if XMC repository can be accessed
FILE(DOWNLOAD ${URL_MSP432}/${XMC} ./${XMC}.found.txt STATUS mystatus)
list(GET mystatus 0 error_code)

if (NOT ${error_code})
add_custom_command(
  OUTPUT ${XMC}
  COMMAND curl https://raw.gitenterprise.xilinx.com/XMC/XMC/XBB-dev/release/${XMC} --output ${XMC}
  COMMENT "Generating binary for XMC"
)

add_custom_target(xmc
 DEPENDS ${XMC}
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/${XMC}
 DESTINATION ${ERT_INSTALL_PREFIX}
 )
else()
  MESSAGE(WARNING "Skipped ${XMC} packaging with error code ${error_code}")
endif()
