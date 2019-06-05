if(NOT WIN32)
# Set INTERNAL_TESTING variable by setting the shell environment 
# variable "AWS_F1_TARGET" when invoking build.sh or cmake. If it
# is not defined, default to INTERNAL_TESTING = 1.
if(DEFINED ENV{AWS_FPGA_REPO_DIR} )
    set(AWS_FPGA_REPO_DIR $ENV{AWS_FPGA_REPO_DIR})
    set(INTERNAL_TESTING 0)
else()
    set(INTERNAL_TESTING 1) 
endif()

add_subdirectory(user_aws)
add_subdirectory(common)
add_subdirectory(linux)
add_subdirectory(tools)
add_subdirectory(driver)
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  # Emulation flow only works on x86_64
  add_subdirectory(emulation)
endif()

else()

add_subdirectory(windows)
  
endif(NOT WIN32)
