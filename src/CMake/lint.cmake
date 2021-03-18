
# Clang Tidy ia integrated into CMake since 3.7.2 and is automatically run if CMAKE_CXX_CLANG_TIDY
# is set. We currently rely on a global .clang-tidy placed inside "src" directory that is automatically
# found by clang-tidy. In future we should refine the checks run for specific directories using
# custom .clang-tidy config files in those directories.

IF((${CMAKE_VERSION} VERSION_GREATER "3.7.2") AND (XRT_CLANG_TIDY STREQUAL "ON"))
  find_program(CLANG_TIDY clang-tidy)
  if(NOT CLANG_TIDY)
    message(FATAL_ERROR "clang-tidy not found, static analysis disabled")
  else()
    message("-- Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  endif()
endif()
