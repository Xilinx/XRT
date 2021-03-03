
# Clang Tidy ia integrated into CMake since 3.7.2 and is automatically run if CMAKE_CXX_CLANG_TIDY
# is set. In future we should refine the checks run using clang-tidy's config files.

IF((${CMAKE_VERSION} VERSION_GREATER "3.7.2") AND (XRT_CLANG_TIDY STREQUAL "ON"))
  find_program(CLANG_TIDY clang-tidy)
  if(NOT CLANG_TIDY)
    message(FATAL_ERROR "clang-tidy not found, static analysis disabled")
  else()
    message("-- Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  endif()
endif()
