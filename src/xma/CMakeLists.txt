option(XMA_VERBOSE "Enable xma verbosity" OFF)

if (XMA_VERBOSE)
  add_compile_options("-DXMA_VERBOSE")
endif()

add_compile_options("-fPIC")
add_compile_options("-Wall" "-Werror")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

add_subdirectory(src)
add_subdirectory(xma_legacy)

#Install all header files with directory structure
#Don't remove trailing/ in include/
install(DIRECTORY include/ DESTINATION ${XMA_INSTALL_DIR}/include/xma2
  PATTERN "include/lib" EXCLUDE
)
