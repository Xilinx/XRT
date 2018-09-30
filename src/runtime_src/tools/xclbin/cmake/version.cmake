# cmake/version.cmake
cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
 
message(STATUS "Resolving Versions...")

set(GIT_BRANCH "Unknown")
set(GIT_XRT_VERSION "Unknown")
set(GIT_LAST_COMMIT_HASH "Unknown")
set(BUILD_DATE "Unknown")
set(GIT_MODIFIED_FILES "Empty")

find_package(Git)
if(GIT_FOUND)
  # Variable: GIT_BRANCH
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  
  # Variable: GIT_XRT_VERSION
  execute_process(
    COMMAND ${GIT_EXECUTABLE} --no-pager describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_XRT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  
  # Variable: GIT_LAST_COMMIT_HASH
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_LAST_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  
  # Variable: GIT_MODIFIED_FILES
  execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_MODIFIED_FILES
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(REPLACE "\n" "," GIT_MODIFIED_FILES "${GIT_MODIFIED_FILES}")
  
  # Variable: BUILD_DATE
  execute_process(
    COMMAND date --iso=seconds
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  message(STATUS "GIT not found")
endif()

configure_file(${local_dir}/cmake/version.h.in ${output_dir}/generated/version.h)

message(STATUS "Generated the file: ${output_dir}/generated/version.h")

