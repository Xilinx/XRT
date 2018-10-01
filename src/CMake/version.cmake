# Get the branch 
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the extended branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} --no-pager describe --tags --always
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH_EXTENDED
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get all of the modified files in the current git environment
execute_process(
  COMMAND ${GIT_EXECUTABLE} status --porcelain
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_MODIFIED_FILES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\n" "," XRT_MODIFIED_FILES "${XRT_MODIFIED_FILES}")


string(TIMESTAMP XRT_DATE "%Y-%m-%d %H:%M:%S")

configure_file(
  ${CMAKE_SOURCE_DIR}/CMake/config/version.h.in
  ${CMAKE_BINARY_DIR}/gen/version.h
)

install(FILES ${CMAKE_BINARY_DIR}/gen/version.h DESTINATION ${XRT_INSTALL_DIR}/include)
