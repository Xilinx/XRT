include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/runtime_src
  )

option(XMA_VERBOSE "Enable xma verbosity" OFF)

if (XMA_VERBOSE)
  add_compile_options("-DXMA_VERBOSE")
endif()

add_compile_options("-fPIC")
add_compile_options("-Wall" "-Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

add_subdirectory(src)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

#Install all header files with directory structure
#Don't remove trailing/ in include/
install(DIRECTORY include/ DESTINATION ${XMA_INSTALL_DIR}/include
  PATTERN "include/lib" EXCLUDE
)
