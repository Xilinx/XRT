execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP XRT_DATE "%Y-%m-%d %H:%M:%S")

configure_file(
  ${CMAKE_SOURCE_DIR}/CMake/config/version.h.in
  ${CMAKE_BINARY_DIR}/gen/version.h
)

install(FILES ${CMAKE_BINARY_DIR}/gen/version.h DESTINATION ${XRT_INSTALL_DIR}/include)
