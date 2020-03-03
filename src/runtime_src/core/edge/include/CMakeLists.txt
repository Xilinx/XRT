set(MPSOC_HEADER_SRC
  xclhal2_mpsoc.h
  sk_types.h)

install (FILES ${MPSOC_HEADER_SRC} DESTINATION ${XRT_INSTALL_INCLUDE_DIR})

message("-- XRT header files for MPSoC only")
message("-- xclhal2_mpsoc.h")
message("-- sk_types.h")

if (DEFINED XRT_AIE_BUILD)
  add_subdirectory(experimental)
endif()
