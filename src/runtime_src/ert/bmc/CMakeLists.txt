# =========== Helper Functions ====================

#------------------------------------------------------------------------------
#    Function: CheckStatus
#
# Description: Checks the return status of the FILE DOWNLOAD operation.  If
#              an error occurs (e.g., can't connect to the server) the error
#              will be reported and build will be stopped.
#               
#------------------------------------------------------------------------------
function(CheckStatus status_code status_error URL_MSP432_GEN)
 if(NOT status_code EQUAL 0)
   message(FATAL_ERROR "ERROR: Failed to retrieve files from: ${URL_MSP432_GEN}\n"
                        "   error code: ${status_code}\n"
                        "   error message: ${status_error}\n")
  endif()
endfunction()

#------------------------------------------------------------------------------
#    Function: GetBMCFWImage
#
# Description: Retrieves the given MSP432 FW image and renames it for the given 
#              Alveo generation.
#               
# Example File Names: 
#               AlveoGen1-BMC-MSP432-4.3.9-35c8bf323af4a5e5a4165d0f2fc29036.txt
#               AlveoGen2-BMC-MSP432-3.6.4-fb8c68be8a6eb1fcf0632966765fcdd3.txt
#               AlveoGen3-BMC-MSP432-4.3.10-0fc12509b3106f6a63bffeb4832576b7.txt
#               AlveoGen4-BMC-MSP432-5.0.27-3fa9028964a7dc81714528ab62e6d40b.txt
#------------------------------------------------------------------------------
function(GetBMCFWImage ALVEOGEN URL_MSP432_GEN)
FILE(DOWNLOAD ${URL_MSP432_GEN}/BMC-MSP432.txt.filename ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN}-BMC-MSP432.txt.filename STATUS mystatus)
  CheckStatus( ${mystatus} ${URL_MSP432_GEN}/BMC-MSP432.txt.filename)

  FILE(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN}-BMC-MSP432.txt.filename ALVEOGEN_FILENAME)

  FILE(DOWNLOAD ${URL_MSP432_GEN}/BMC-MSP432.txt ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN}-${ALVEOGEN_FILENAME} STATUS mystatus)
  CheckStatus( ${mystatus} ${URL_MSP432_GEN}/BMC-MSP432.txt)

  # Add this FW image to the list of FW images to be released
  list(APPEND ALVEO_FW_FILES ${CMAKE_CURRENT_BINARY_DIR}/${ALVEOGEN}-${ALVEOGEN_FILENAME}) 

  # Rescope the list to the parent domain
  set (ALVEO_FW_FILES  ${ALVEO_FW_FILES} PARENT_SCOPE)
endfunction()

# ============= Alveo FW Images ==================
GetBMCFWImage( "AlveoGen1" "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master/BMC-card-firmware" )
GetBMCFWImage( "AlveoGen2" "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/master-U280/BMC-card-firmware" )
GetBMCFWImage( "AlveoGen3" "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/U280-prod/BMC-card-firmware" )
GetBMCFWImage( "AlveoGen4" "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/U50/BMC-card-firmware")
GetBMCFWImage( "AlveoGen5" "https://raw.gitenterprise.xilinx.com/XBB/BMC-firmware-binaries/V350/BMC-card-firmware")

# ================= INSTALL FIRMWARE IMAGES =========================
install (FILES ${ALVEO_FW_FILES} DESTINATION ${ERT_INSTALL_PREFIX})

