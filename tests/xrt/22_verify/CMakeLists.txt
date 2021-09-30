# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "22_verify")

add_executable(${TESTNAME} main.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(${TESTNAME}_xcl xcl.cpp)
target_link_libraries(${TESTNAME}_xcl PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})

# Experimental demo of static linking.
# Should be built into to PUBLIC options along with find_package
if (DEFINED ENV{XRT_STATIC_BUILD})
  add_executable(${TESTNAME}_hw_static main.cpp)
  if (NOT WIN32)
    target_link_options(${TESTNAME}_hw_static PRIVATE "-static" "-L${XRT_LINK_DIRS}")
    target_link_libraries(${TESTNAME}_hw_static
      PRIVATE
      # force linking of whole archive to enable static globals
      -Wl,--whole-archive -lxrt_core_static -Wl,--no-whole-archive
      -Wl,--whole-archive -lrt -lpthread -Wl,--no-whole-archive
      )
  endif()
  target_link_libraries(${TESTNAME}_hw_static
    PRIVATE
    ${xrt_core_static_LIBRARY}
    ${xrt_coreutil_static_LIBRARY}
    )

  install(TARGETS ${TESTNAME}_hw_static
    RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
endif()

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(${TESTNAME}_xcl PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME} ${TESTNAME}_xcl
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
