# Set INTERNAL_TESTING variable by setting the shell environment 
# variable "AWS_F1_TARGET" when invoking build.sh or cmake. If it
# is not defined, default to INTERNAL_TESTING = 1.
if( $ENV{AWS_FPGA_REPO_DIR} )
    set(INTERNAL_TESTING 0)
else()
    set(INTERNAL_TESTING 1) 
endif()
add_subdirectory(xrt)
add_subdirectory(tools/xbflash)
add_subdirectory(tools/xbutil)
add_subdirectory(tools/awssak)

