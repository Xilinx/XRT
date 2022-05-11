# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
add_subdirectory(include)

set(XRT_KERNEL_HEADERS
  include/xocl_ioctl.h
)

set(QDMA_LIB_DESTINATION_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/xocl/lib/libqdma/
)

function(CopyQDMALIB)
 message(STATUS "Copying QDMA lib file from $ENV{QDMA_LIB_DIR}")
 file(COPY $ENV{QDMA_LIB_DIR} DESTINATION ${QDMA_LIB_DESTINATION_DIR})
endfunction()

if(DEFINED ENV{QDMA_LIB_DIR})
 CopyQDMALIB()
endif()

install(FILES ${XRT_KERNEL_HEADERS} DESTINATION ${XRT_INSTALL_DIR}/include)

