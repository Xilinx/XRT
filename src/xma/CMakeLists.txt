include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  "../runtime_src/driver/xclng/include"
  "../runtime_src/driver/include"
  )

option(XMA_VERBOSE "Enable xma verbosity" OFF)

if (XMA_VERBOSE)
  add_compile_options("-DXMA_VERBOSE")
endif()

add_compile_options("-fPIC")
add_compile_options("-Wall" "-Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DXMA_RES_TEST -DXMA_DEBUG")

#Uncomment when dep xml2 yaml are installed
#add_subdirectory(src)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")

#Install all header files with directory structure
#Don't remove trailing/ in include/
install(DIRECTORY include/ DESTINATION ${XMA_INSTALL_DIR}/include)

