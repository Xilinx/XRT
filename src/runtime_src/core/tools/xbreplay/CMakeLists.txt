# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved

# -----------------------------------------------------------------------------
# Include the generated header files (e.g., version.h)
include_directories(${XRT_BINARY_DIR}/gen)

if(WIN32)
  set(XRT_HELPER_SCRIPTS "xbreplay" "xbreplay.bat")
else()
  set(XRT_HELPER_SCRIPTS "xbreplay")
endif()

add_library(xbreplay_objects OBJECT
  src/utils/cmd_args.cpp
  src/utils/message.cpp
  src/seq_reconstructor/seq_reconstructor.cpp
  src/replay_eng/replay.cpp
  src/replay_xrt/replay_xrt_bo.cpp
  src/replay_xrt/replay_xrt_device.cpp
  src/replay_xrt/replay_xrt_hwctx.cpp
  src/replay_xrt/replay_xrt_kernel.cpp
  src/replay_xrt/replay_xrt_run.cpp
  src/replay_xrt/replay_xrt_xclbin.cpp
  src/replay_xrt/replay_xrt_module.cpp
  src/replay_xrt/replay_xrt_elf.cpp
)

target_include_directories(xbreplay_objects PRIVATE
  src/seq_reconstructor
  src/replay_eng
  src/replay_xrt
  src
)

add_executable(xbreplay
  src/main.cpp
  $<TARGET_OBJECTS:xbreplay_objects>
)

target_include_directories(xbreplay PRIVATE
  src/seq_reconstructor
  src/replay_eng
  src/replay_xrt
  src
)

target_link_libraries(xbreplay
  PRIVATE
  xrt_coreutil
)

if (NOT WIN32)
  target_link_libraries(xbreplay PRIVATE pthread dl PUBLIC uuid)
endif()

install (TARGETS xbreplay
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})
install (PROGRAMS ${XRT_HELPER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})

