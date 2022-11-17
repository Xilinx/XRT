# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#

# =========================================================
# The plugins to be built on both Linux and Windows
# =========================================================
add_subdirectory(device_offload/opencl)
add_subdirectory(device_offload/hal)
add_subdirectory(hal)
add_subdirectory(hal_api_interface)
add_subdirectory(lop)
add_subdirectory(native)
add_subdirectory(opencl/counters)
add_subdirectory(opencl/trace)
add_subdirectory(pl_deadlock)
add_subdirectory(user)

if (NOT WIN32)
# =========================================================
# The plugins to be built on Linux only
# =========================================================
add_subdirectory(aie_debug)
add_subdirectory(aie_profile)
add_subdirectory(aie_trace_new)
add_subdirectory(device_offload/hw_emu)
add_subdirectory(noc)
add_subdirectory(power)
add_subdirectory(system_compiler)
add_subdirectory(vart)

else()
# =========================================================
# The plugins to be built on Windows only (currently empty)
# =========================================================

endif()
