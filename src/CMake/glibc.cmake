# get glibc version for use in CPACK_*_XRT_PACKAGE_DEPENDS
# source: https://gist.github.com/likema/f5c04dad837d2f5068ae7a8860c180e7
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.so.6
    OUTPUT_VARIABLE GLIBC
    OUTPUT_STRIP_TRAILING_WHITESPACE)
get_filename_component(GLIBC ${GLIBC} REALPATH)
get_filename_component(GLIBC_VERSION ${GLIBC} NAME)
string(REPLACE "libc-" "" GLIBC_VERSION ${GLIBC_VERSION})
string(REPLACE ".so" "" GLIBC_VERSION ${GLIBC_VERSION})
if(NOT GLIBC_VERSION MATCHES "^[0-9.]+$")
    message(FATAL_ERROR "Unknown glibc version: ${GLIBC_VERSION}")
endif(NOT GLIBC_VERSION MATCHES "^[0-9.]+$")
