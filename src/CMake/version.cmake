# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

# AMD promotion build works from copied sources with no git
# repository.  The build cannot query git for git metadata.  The
# promotion build has explicitly overwritten config/version.h.in and
# config/version.json.in with pre-generated ones.
if (DEFINED ENV{DK_ROOT})

message("-- Skipping Git metadata")

else (DEFINED ENV{DK_ROOT})

# Get the branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get number of commits for HEAD
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HEAD_COMMITS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

#Set XRT_HEAD_COMMITS to default value if above command is not executed
if (NOT XRT_HEAD_COMMITS)
set (XRT_HEAD_COMMITS -1)
endif()

# Get number of commits between HEAD and master
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD ^origin/master
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_BRANCH_COMMITS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

#Set XRT_BRANCH_COMMITS to default value if above command is not executed
if (NOT XRT_BRANCH_COMMITS)
set (XRT_BRANCH_COMMITS -1)
endif()

# Get the latest abbreviated commit hash date of the working branch
execute_process(
  COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%cD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_HASH_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get all of the modified files in the current git environment
execute_process(
  COMMAND ${GIT_EXECUTABLE} status --porcelain -u no
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_MODIFIED_FILES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\n" "," XRT_MODIFIED_FILES "${XRT_MODIFIED_FILES}")

endif(DEFINED ENV{DK_ROOT})

# Get the build date RFC format
execute_process(
  COMMAND date -R
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE XRT_DATE_RFC
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP XRT_DATE "%Y-%m-%d %H:%M:%S")

configure_file(
  ${XRT_SOURCE_DIR}/CMake/config/version-slim.h.in
  ${PROJECT_BINARY_DIR}/gen/xrt/detail/version-slim.h
)

configure_file(
  ${XRT_SOURCE_DIR}/CMake/config/version.h.in
  ${PROJECT_BINARY_DIR}/gen/xrt/detail/version.h
)

configure_file(
  ${XRT_SOURCE_DIR}/CMake/config/version.json.in
  ${PROJECT_BINARY_DIR}/gen/version.json
)

if (WIN32 AND XRT_RC_VERSION)
  # break up fileversion into 4 components
  string(REPLACE "." ";" XRT_RC_VERSION_LIST ${XRT_RC_VERSION})
  list(LENGTH XRT_RC_VERSION_LIST XRT_RC_VERSION_COUNT)
  if (NOT XRT_RC_VERSION_COUNT EQUAL 4)
    message(FATAL_ERROR "XRT_RC_VERSION must be in the form of 'major.minor.patch.build'")
  endif()

  list(GET XRT_RC_VERSION_LIST 0 XRT_RC_MAJOR)
  list(GET XRT_RC_VERSION_LIST 1 XRT_RC_MINOR)
  list(GET XRT_RC_VERSION_LIST 2 XRT_RC_BUILD)
  list(GET XRT_RC_VERSION_LIST 3 XRT_RC_PATCH)

  function(xrt_configure_version_file target_name type)
    if (type STREQUAL "SHARED")
      set(OriginalFilename ${target_name}.dll)
      set(FileType VFT_DLL)
    elseif(type STREQUAL "APP")
      set(OriginalFilename ${target_name}.exe)
      set(FileType VFT_APP)
    else()
      message(FATAL_ERROR "Unknown file type ${type} for version file configuration")
    endif()

    configure_file(
      ${XRT_SOURCE_DIR}/CMake/config/version.rc.in
      ${CMAKE_CURRENT_BINARY_DIR}/${target_name}-version.rc
      @ONLY
      )
  endfunction()
else()
  function(xrt_configure_version_file target_name type)
    file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/${target_name}-version.rc)
  endfunction()
endif()  

# xrt component install
install(FILES
  ${PROJECT_BINARY_DIR}/gen/xrt/detail/version.h
  ${PROJECT_BINARY_DIR}/gen/xrt/detail/version-slim.h
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

if (NOT XRT_EDGE)
  install(FILES ${PROJECT_BINARY_DIR}/gen/version.json
    DESTINATION ${XRT_INSTALL_DIR}
    COMPONENT ${XRT_BASE_COMPONENT})
endif()

# This is not required on MPSoC platform. To avoid yocto error, do NOT intall
if (XRT_ALVEO AND (NOT XRT_EDGE) AND (NOT WIN32))
  # Copied over from dkms.cmake. TODO: cleanup
  set (XRT_DKMS_INSTALL_DIR "/usr/src/xrt-${XRT_VERSION_STRING}")
  install(FILES
    ${PROJECT_BINARY_DIR}/gen/xrt/detail/version.h
    ${PROJECT_BINARY_DIR}/gen/xrt/detail/version-slim.h
    DESTINATION ${XRT_DKMS_INSTALL_DIR}/driver/include
    COMPONENT ${XRT_DEV_COMPONENT})
  install(FILES
    ${PROJECT_BINARY_DIR}/gen/xrt/detail/version-slim.h
    DESTINATION ${XRT_DKMS_INSTALL_DIR}/driver/include/xrt/detail
    COMPONENT ${XRT_DEV_COMPONENT})
endif()
