include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(include)
add_subdirectory(common)
 if (${XRT_NATIVE_BUILD} STREQUAL "yes")
   add_subdirectory(pcie)
   add_subdirectory(tools)
 else()
   add_subdirectory(edge)
   add_subdirectory(tools)
 endif()
