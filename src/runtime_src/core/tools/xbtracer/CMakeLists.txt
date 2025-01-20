# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved
# -----------------------------------------------------------------------------
# Include the generated header files (e.g., version.h)
include_directories(${XRT_BINARY_DIR}/gen)

set(XBTRACER_NAME "xbtracer")

if(WIN32)
  set(XRT_HELPER_SCRIPTS "xbtracer" "xbtracer.bat")
else()
  set(XRT_HELPER_SCRIPTS "xbtracer")
endif()

set(SRCS src/app/launcher.cpp)
if (WIN32)
  list(APPEND SRCS src/app/getopt.c)
endif()

add_executable(${XBTRACER_NAME} ${SRCS})

# Static build is a Linux / Ubuntu option only
if (XRT_STATIC_BUILD)
  add_executable(${XBTRACER_NAME}_static ${SRCS})
  target_link_options(${XBTRACER_NAME}_static PRIVATE "-static" "-L${Boost_LIBRARY_DIRS}")
  # Bypass FindBoost versions and just link explicitly with boost libraries
  # The -static link option will pick the static libraries.
  target_link_libraries(${XBTRACER_NAME}_static
    PRIVATE
    xrt_coreutil_static
    boost_system
    boost_program_options
    -Wl,--whole-archive rt pthread -Wl,--no-whole-archive
    uuid
    dl
    )
  set_target_properties(${XBTRACER_NAME}_static PROPERTIES INSTALL_RPATH "")
  install(TARGETS ${XBTRACER_NAME}_static
    RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
endif()

install(TARGETS ${XBTRACER_NAME}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})

install(PROGRAMS ${XRT_HELPER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR}
  COMPONENT ${XRT_BASE_COMPONENT})

add_subdirectory(src/lib)
