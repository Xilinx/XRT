cmake_minimum_required(VERSION 3.10)
project(checkCppMangledNames)
#if (WIN32)
#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON) # Alternatively, use the method with macro
#endif (WIN32)

set(CMAKE_MESSAGE_CONTEXT_SHOW True)

set(ENV{XRT_BOOST_INSTALL} "${XRT_BOOST_INSTALL}")
set(XRT_INSTALL_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(XRT_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR})
include (${XRT_SOURCE_DIR}/CMake/settings.cmake)
include (${XRT_SOURCE_DIR}/CMake/boostUtil.cmake)
include_directories(${Boost_INCLUDE_DIRS})
add_compile_definitions("BOOST_LOCALE_HIDE_AUTO_PTR")
add_compile_definitions("BOOST_BIND_GLOBAL_PLACEHOLDERS")
include (${XRT_SOURCE_DIR}/CMake/version.cmake)

if (MSVC)
  # Static linking with the CRT
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  add_compile_options(
    /MT$<$<CONFIG:Debug>:d>  # static linking with the CRT
    /Zc:__cplusplus
    /Zi           # generate pdb files even in release mode
    /sdl          # enable security checks
    /Qspectre     # compile with the Spectre mitigations switch
    /ZH:SHA_256   # enable secure source code hashing
    /guard:cf     # enable compiler control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    )
  add_link_options(
    /NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib  # Hybrid CRT
    /DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib       # Hybrid CRT
    /DEBUG      # instruct linker to create debugging info
    /guard:cf   # enable linker control guard feature (CFG) to prevent attackers from redirecting execution to unsafe locations
    /CETCOMPAT  # enable Control-flow Enforcement Technology (CET) Shadow Stack mitigation
    )
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
message("SRC_DIR=${SRC_DIR}")
file(GLOB_RECURSE CPP_FILES *.cpp)
add_library(gen_tmp SHARED ${CPP_FILES})
target_include_directories(gen_tmp PRIVATE ${XRT_SOURCE_DIR}/runtime_src/core/include ${CMAKE_CURRENT_BINARY_DIR}/gen ${XRT_SOURCE_DIR}/runtime_src)
target_compile_options(gen_tmp PRIVATE -DXCL_DRIVER_DLL_EXPORT -DXRT_API_SOURCE)

set_source_files_properties(${CPP_FILES} PROPERTIES GENERATED 1)
