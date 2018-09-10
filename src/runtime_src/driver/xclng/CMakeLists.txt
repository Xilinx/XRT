# Set INTERNAL_TESTING variable by setting the shell environment 
# variable "AWS_F1_TARGET" when invoking build.sh or cmake. If it
# is not defined, default to INTERNAL_TESTING = 1.
if( $ENV{AWS_F1_TARGET} )
    set(INTERNAL_TESTING 0)
else()
    set(INTERNAL_TESTING 1) #AWS_INTERNAL_TESTING
endif()

add_subdirectory(xrt)
add_subdirectory(tools/xbflash)
add_subdirectory(tools/xbutil)
add_subdirectory(tools/awssak)
