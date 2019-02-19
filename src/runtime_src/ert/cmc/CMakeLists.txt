set (CMC "cmc.bin")
set (URL_MSP432 "https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release")

# Check if CMC repository can be accessed
FILE(DOWNLOAD ${URL_MSP432}/${CMC} ./${CMC}.found.txt STATUS mystatus)
list(GET mystatus 0 error_code)

if (NOT ${error_code})
add_custom_command(
  OUTPUT ${CMC}
  COMMAND curl https://raw.gitenterprise.xilinx.com/CMC/CMC/master/release/${CMC} --output ${CMC}
  COMMENT "Generating binary for CMC"
)

add_custom_target(cmc
 DEPENDS ${CMC}
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/${CMC}
 DESTINATION ${ERT_INSTALL_PREFIX}
 )
else()
  MESSAGE(WARNING "Skipped ${CMC} packaging with error code ${error_code}")
endif()
