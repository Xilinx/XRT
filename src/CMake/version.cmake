# Get the branch 
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash date of the working branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%cD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH_DATE
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

# Get the build date RFC format
execute_process(
  COMMAND date -R
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_DATE_RFC
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


string(TIMESTAMP XRT_DATE "%Y-%m-%d %H:%M:%S")


configure_file(
  ${CMAKE_SOURCE_DIR}/CMake/config/version.h.in
  ${CMAKE_BINARY_DIR}/gen/version.h
)

configure_file(
  ${CMAKE_SOURCE_DIR}/CMake/config/version.json.in
  ${CMAKE_BINARY_DIR}/gen/version.json
)

install(FILES ${CMAKE_BINARY_DIR}/gen/version.h DESTINATION ${XRT_INSTALL_DIR}/include)
install(FILES ${CMAKE_BINARY_DIR}/gen/version.json DESTINATION ${XRT_INSTALL_DIR})

# Copied over from dkms.cmake. TODO: cleanup
set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")
install(FILES ${CMAKE_BINARY_DIR}/gen/version.h DESTINATION ${XRT_DKMS_INSTALL_DIR}/driver/include)
