# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
if (NOT WIN32)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${XRT_SOURCE_DIR}/include/1_2
  )

else()
# = WINDOWS ===================================================================

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${XRT_SOURCE_DIR}/include/1_2
  ${KHRONOS}/include
  )
endif()

# ====== File directory definitions for both Linux and Windows =========

set(XRT_XDP_APPDEBUG_DIR               "${CMAKE_CURRENT_SOURCE_DIR}/appdebug")
set(XRT_XDP_DEBUG_DIR                  "${CMAKE_CURRENT_SOURCE_DIR}/debug")
set(XRT_XDP_PROFILE_DIR                "${CMAKE_CURRENT_SOURCE_DIR}/profile")
set(XRT_XDP_PROFILE_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin")
set(XRT_XDP_PROFILE_OCL_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/ocl")
set(XRT_XDP_PROFILE_HAL_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/hal")
set(XRT_XDP_PROFILE_HAL_API_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/hal_api_interface")
set(XRT_XDP_PROFILE_AIE_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie_profile")
set(XRT_XDP_PROFILE_AIE_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/aie_profile")
set(XRT_XDP_PROFILE_NOC_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/noc")
set(XRT_XDP_PROFILE_NOC_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/noc")
set(XRT_XDP_PROFILE_VART_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/vart")
set(XRT_XDP_PROFILE_COLLECTION_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/collection")
set(XRT_XDP_PROFILE_WRITER_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer")
set(XRT_XDP_PROFILE_HAL_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/hal")
set(XRT_XDP_PROFILE_CORE_DIR           "${CMAKE_CURRENT_SOURCE_DIR}/profile/core")
set(XRT_XDP_PROFILE_DEVICE_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/device")
set(XRT_XDP_PROFILE_DEVICE_MMAPPED_MON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/mmapped_monitors")
set(XRT_XDP_PROFILE_DEVICE_IOCTL_MON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/ioctl_monitors")
set(XRT_XDP_PROFILE_OCL_DEVICE_LOGGER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/ocl_device_logger")
set(XRT_XDP_PROFILE_HAL_DEVICE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/hal_device")
set(XRT_XDP_PROFILE_XRT_DEVICE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/xrt_device")
set(XRT_XDP_PROFILE_LOP_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/lop")
set(XRT_XDP_PROFILE_LOP_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/lop")
set(XRT_XDP_PROFILE_NATIVE_PLUGIN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/native")
set(XRT_XDP_PROFILE_NATIVE_WRITER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/native")
set(XRT_XDP_PROFILE_OPENCL_TRACE_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/opencl/trace")
set(XRT_XDP_PROFILE_OPENCL_TRACE_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/opencl")
set(XRT_XDP_PROFILE_OPENCL_COUNTERS_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/opencl/counters")
set(XRT_XDP_PROFILE_OPENCL_COUNTERS_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/opencl")
set(XRT_XDP_PROFILE_USER_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/user")
set(XRT_XDP_PROFILE_USER_WRITER_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/user")
set(XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload")
set(XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/device_trace")
set(XRT_XDP_PROFILE_OPENCL_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload/opencl")
set(XRT_XDP_PROFILE_HAL_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload/hal")
set(XRT_XDP_PROFILE_HW_EMU_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload/hw_emu")
set(XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/aie_trace")
set(XRT_XDP_AIE_TRACE_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie_trace")
set(XRT_XDP_AIE_TRACE_WRITER_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/aie_trace")
set(XRT_XDP_AIE_DEBUG_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie_debug")
set(XRT_XDP_AIE_DEBUG_WRITER_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/aie_debug")
set(XRT_XDP_POWER_PROFILE_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/power")
set(XRT_XDP_POWER_PROFILE_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/power")
set(XRT_XDP_SYSTEM_COMPILER_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/system_compiler")
set(XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/pl_deadlock")
set(XRT_XDP_AIE_TRACE_NEW_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie_trace_new")

# Files for the new xdp core
set(XRT_XDP_PROFILE_BASE_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/vp_base")
set(XRT_XDP_PROFILE_DATABASE_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/profile/database")
set(XRT_XDP_PROFILE_EVENTS_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/database/events")
set(XRT_XDP_PROFILE_STATIC_INFO_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/database/static_info")
set(XRT_XDP_PROFILE_EVENTS_CREATOR_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/database/events/creator")
set(XRT_XDP_PROFILE_VP_WRITER_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/vp_base")

file(GLOB XRT_NEW_XDP_CORE_FILES
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_MMAPPED_MON_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_MMAPPED_MON_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_IOCTL_MON_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_IOCTL_MON_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.h"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_STATIC_INFO_DIR}/*.h"
  "${XRT_XDP_PROFILE_STATIC_INFO_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_EVENTS_CREATOR_DIR}/*.h"
  "${XRT_XDP_PROFILE_EVENTS_CREATOR_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_APPDEBUG_FILES
  "${XRT_XDP_APPDEBUG_DIR}/*.h"
  "${XRT_XDP_APPDEBUG_DIR}/*.cpp"
  )

if (NOT WIN32)
  set_source_files_properties (${XRT_XDP_APPDEBUG_FILES} PROPERTIES COMPILE_FLAGS -g)
endif()

file(GLOB XRT_XDP_DEBUG_FILES
  "${XRT_XDP_DEBUG_DIR}/*.h"
  "${XRT_XDP_DEBUG_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_FILES
  "${XRT_XDP_PROFILE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_COLLECTION_DIR}/*.h"
  "${XRT_XDP_PROFILE_COLLECTION_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_WRITER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_CORE_DIR}/*.h"
  "${XRT_XDP_PROFILE_CORE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_MMAPPED_MON_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_MMAPPED_MON_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_IOCTL_MON_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_IOCTL_MON_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OCL_DEVICE_LOGGER_DIR}/*.h"
  "${XRT_XDP_PROFILE_OCL_DEVICE_LOGGER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_OCL_PLUGIN_FILES
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OCL_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OCL_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_HAL_PLUGIN_FILES
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_API_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_API_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_AIE_PLUGIN_FILES
  "${XRT_XDP_PROFILE_AIE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_AIE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_AIE_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_AIE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_NOC_PLUGIN_FILES
  "${XRT_XDP_PROFILE_NOC_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_NOC_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_NOC_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_NOC_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_LOP_PLUGIN_FILES
  "${XRT_XDP_PROFILE_LOP_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_LOP_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_LOP_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_LOP_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_NATIVE_PLUGIN_FILES
  "${XRT_XDP_PROFILE_NATIVE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_NATIVE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_NATIVE_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_NATIVE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_OPENCL_TRACE_PLUGIN_FILES
  "${XRT_XDP_PROFILE_OPENCL_TRACE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OPENCL_TRACE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OPENCL_TRACE_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_OPENCL_TRACE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_OPENCL_COUNTERS_PLUGIN_FILES
  "${XRT_XDP_PROFILE_OPENCL_COUNTERS_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OPENCL_COUNTERS_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OPENCL_COUNTERS_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_OPENCL_COUNTERS_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_USER_PLUGIN_FILES
  "${XRT_XDP_PROFILE_USER_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_USER_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_USER_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_USER_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_OPENCL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OPENCL_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OPENCL_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_HAL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_HW_EMU_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HW_EMU_DEVICE_OFFLOAD_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HW_EMU_DEVICE_OFFLOAD_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_POWER_PROFILE_PLUGIN_FILES
  "${XRT_XDP_POWER_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_POWER_PROFILE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_POWER_PROFILE_WRITER_DIR}/*.h"
  "${XRT_XDP_POWER_PROFILE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_SYSTEM_COMPILER_PLUGIN_FILES
  "${XRT_XDP_SYSTEM_COMPILER_PLUGIN_DIR}/*.h"
  "${XRT_XDP_SYSTEM_COMPILER_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_AIE_TRACE_PLUGIN_FILES
  "${XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_AIE_TRACE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_AIE_TRACE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_AIE_TRACE_WRITER_DIR}/*.h"
  "${XRT_XDP_AIE_TRACE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_AIE_TRACE_NEW_PLUGIN_FILES
  "${XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_AIE_TRACE_NEW_PLUGIN_DIR}/*.h"
  "${XRT_XDP_AIE_TRACE_NEW_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_AIE_TRACE_NEW_PLUGIN_DIR}/x86/*.h"
  "${XRT_XDP_AIE_TRACE_NEW_PLUGIN_DIR}/x86/*.cpp"
  "${XRT_XDP_AIE_TRACE_WRITER_DIR}/*.h"
  "${XRT_XDP_AIE_TRACE_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_AIE_DEBUG_PLUGIN_FILES
  "${XRT_XDP_AIE_DEBUG_PLUGIN_DIR}/*.h"
  "${XRT_XDP_AIE_DEBUG_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_AIE_DEBUG_WRITER_DIR}/*.h"
  "${XRT_XDP_AIE_DEBUG_WRITER_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_VART_PLUGIN_FILES
  "${XRT_XDP_PROFILE_VART_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_VART_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_FILES
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_HW_EMU_PROFILE_PL_DEADLOCK_PLUGIN_FILES
  "${XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_DIR}/hw_emu/*.h"
  "${XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_DIR}/hw_emu/*.cpp"
  )

set(XRT_XDP_ALL_SRC
  ${XRT_XDP_PROFILE_FILES}
  ${XRT_XDP_FILES}
  )

# ========== XDP Core Library For Both Linux and Windows =============

add_library(xdp_core SHARED ${XRT_NEW_XDP_CORE_FILES})
add_dependencies(xdp_core xrt_coreutil)
target_link_libraries(xdp_core PRIVATE xrt_coreutil)

set_target_properties(xdp_core PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_core
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install (TARGETS xdp_core
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)

# ============ XDP Plugin Modules For Both Linux and Windows ===========

add_library(xdp_hal_plugin MODULE ${XRT_XDP_PROFILE_HAL_PLUGIN_FILES})
add_library(xdp_hal_api_interface_plugin MODULE ${XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES})
add_library(xdp_lop_plugin MODULE ${XRT_XDP_PROFILE_LOP_PLUGIN_FILES})
add_library(xdp_native_plugin MODULE ${XRT_XDP_PROFILE_NATIVE_PLUGIN_FILES})
add_library(xdp_opencl_trace_plugin MODULE ${XRT_XDP_PROFILE_OPENCL_TRACE_PLUGIN_FILES})
add_library(xdp_opencl_counters_plugin MODULE ${XRT_XDP_PROFILE_OPENCL_COUNTERS_PLUGIN_FILES})
add_library(xdp_user_plugin MODULE ${XRT_XDP_PROFILE_USER_PLUGIN_FILES})
add_library(xdp_device_offload_plugin MODULE ${XRT_XDP_OPENCL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES})
add_library(xdp_hal_device_offload_plugin MODULE ${XRT_XDP_HAL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES})
add_library(xdp_pl_deadlock_plugin MODULE ${XRT_XDP_PROFILE_PL_DEADLOCK_PLUGIN_FILES})

add_dependencies(xdp_hal_plugin xdp_core xrt_coreutil)
add_dependencies(xdp_hal_api_interface_plugin xdp_core xrt_core xrt_coreutil)
add_dependencies(xdp_lop_plugin xdp_core xrt_coreutil)
add_dependencies(xdp_native_plugin xdp_core xrt_coreutil)
add_dependencies(xdp_opencl_trace_plugin xdp_core xrt_coreutil)
add_dependencies(xdp_opencl_counters_plugin xdp_core xrt_coreutil xilinxopencl)
add_dependencies(xdp_user_plugin xdp_core xrt_coreutil)
add_dependencies(xdp_device_offload_plugin xdp_core xilinxopencl xrt_coreutil)
add_dependencies(xdp_hal_device_offload_plugin xdp_core xrt_core xrt_coreutil)
add_dependencies(xdp_pl_deadlock_plugin xdp_core xrt_core xrt_coreutil)

target_link_libraries(xdp_hal_plugin PRIVATE xdp_core xrt_coreutil)
target_link_libraries(xdp_hal_api_interface_plugin PRIVATE xdp_core xrt_core xrt_coreutil)
target_link_libraries(xdp_lop_plugin PRIVATE xdp_core xrt_coreutil)
target_link_libraries(xdp_native_plugin PRIVATE xdp_core xrt_coreutil)
target_link_libraries(xdp_opencl_trace_plugin PRIVATE xdp_core xrt_coreutil)
target_link_libraries(xdp_opencl_counters_plugin PRIVATE xdp_core xrt_coreutil xilinxopencl)
target_link_libraries(xdp_user_plugin PRIVATE xdp_core xrt_coreutil)
target_link_libraries(xdp_device_offload_plugin PRIVATE xdp_core xilinxopencl xrt_coreutil)
target_link_libraries(xdp_hal_device_offload_plugin PRIVATE xdp_core xrt_core xrt_coreutil)
target_link_libraries(xdp_pl_deadlock_plugin PRIVATE xdp_core xrt_core xrt_coreutil)

set_target_properties(xdp_hal_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_api_interface_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_lop_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_native_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_opencl_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_opencl_counters_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_user_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_pl_deadlock_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

# Install directories are different for Windows and Linux

if (NOT WIN32)

install (TARGETS xdp_hal_plugin
                 xdp_hal_api_interface_plugin
                 xdp_lop_plugin
                 xdp_native_plugin
                 xdp_opencl_trace_plugin
                 xdp_opencl_counters_plugin
                 xdp_user_plugin
                 xdp_device_offload_plugin
                 xdp_hal_device_offload_plugin
                 xdp_pl_deadlock_plugin
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
)

else()

install (TARGETS xdp_hal_plugin
                 xdp_hal_api_interface_plugin
                 xdp_lop_plugin
                 xdp_native_plugin
                 xdp_opencl_trace_plugin
                 xdp_opencl_counters_plugin
                 xdp_user_plugin
                 xdp_device_offload_plugin
                 xdp_hal_device_offload_plugin
                 xdp_pl_deadlock_plugin
  LIBRARY DESTINATION ${XRT_INSTALL_BIN_DIR}
)

endif()

# ============ Linux Specific Plugin modules =============
if (NOT WIN32)

add_library(xdp_debug_plugin MODULE ${XRT_XDP_DEBUG_FILES})
add_library(xdp_appdebug_plugin MODULE ${XRT_XDP_APPDEBUG_FILES})
add_library(xdp_power_plugin MODULE ${XRT_XDP_POWER_PROFILE_PLUGIN_FILES})
add_library(xdp_system_compiler_plugin MODULE ${XRT_XDP_SYSTEM_COMPILER_PLUGIN_FILES})
add_library(xdp_noc_plugin MODULE ${XRT_XDP_PROFILE_NOC_PLUGIN_FILES})
add_library(xdp_vart_plugin MODULE ${XRT_XDP_PROFILE_VART_PLUGIN_FILES})
add_library(xdp_hw_emu_device_offload_plugin MODULE ${XRT_XDP_HW_EMU_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES})
add_library(xdp_hw_emu_pl_deadlock_plugin MODULE ${XRT_XDP_HW_EMU_PROFILE_PL_DEADLOCK_PLUGIN_FILES})

add_dependencies(xdp_debug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_power_plugin xdp_core xrt_core)
add_dependencies(xdp_system_compiler_plugin xdp_core)
add_dependencies(xdp_noc_plugin xdp_core xrt_core)
add_dependencies(xdp_vart_plugin xdp_core xrt_core)
add_dependencies(xdp_hw_emu_device_offload_plugin xdp_core xrt_coreutil xrt_hwemu)
add_dependencies(xdp_hw_emu_pl_deadlock_plugin xdp_core xrt_coreutil xrt_hwemu)

target_link_libraries(xdp_debug_plugin PRIVATE xrt_coreutil xilinxopencl)
target_link_libraries(xdp_appdebug_plugin PRIVATE xrt_coreutil xilinxopencl)
target_link_libraries(xdp_power_plugin PRIVATE xdp_core xrt_core)
target_link_libraries(xdp_system_compiler_plugin xdp_core)
target_link_libraries(xdp_noc_plugin PRIVATE xdp_core xrt_core)
target_link_libraries(xdp_vart_plugin PRIVATE xdp_core xrt_core)
target_link_libraries(xdp_hw_emu_device_offload_plugin PRIVATE xdp_core xrt_coreutil xrt_hwemu)
target_link_libraries(xdp_hw_emu_pl_deadlock_plugin PRIVATE xdp_core xrt_coreutil xrt_hwemu)

set_target_properties(xdp_noc_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_vart_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_appdebug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_power_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_system_compiler_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hw_emu_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hw_emu_pl_deadlock_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

# x86 AI Engine Plugin
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_library(xdp_aie_trace_plugin MODULE ${XRT_XDP_AIE_TRACE_NEW_PLUGIN_FILES})
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_core)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
  target_compile_definitions(xdp_aie_trace_plugin PRIVATE XRT_NATIVE_BUILD=1)
  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
  )
endif()

# AI Engine plugins are Edge only.
# With x86 sw_emu support of AIE, XRT_AIE_BUILD is defined even on x86
# so the condition is strengthen to include native build not defined,
# which is same condition that compiles edge.
if (DEFINED XRT_AIE_BUILD AND ${XRT_NATIVE_BUILD} STREQUAL "no")
  # AIE Profile plugin
  add_library(xdp_aie_profile_plugin MODULE ${XRT_XDP_PROFILE_AIE_PLUGIN_FILES})
  add_dependencies(xdp_aie_profile_plugin xdp_core)
  target_link_libraries(xdp_aie_profile_plugin PRIVATE xdp_core metal xaiengine)
  set_target_properties(xdp_aie_profile_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_profile_plugin
    LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
  )

  # AIE Trace plugin
  add_library(xdp_aie_trace_plugin MODULE ${XRT_XDP_AIE_TRACE_PLUGIN_FILES})
  add_dependencies(xdp_aie_trace_plugin xdp_core xrt_core)
  target_link_libraries(xdp_aie_trace_plugin PRIVATE xdp_core xrt_core metal xaiengine)
  set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_trace_plugin
    LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
  )

  # AIE Debug plugin
  add_library(xdp_aie_debug_plugin MODULE ${XRT_XDP_AIE_DEBUG_PLUGIN_FILES})
  add_dependencies(xdp_aie_debug_plugin xdp_core)
  target_link_libraries(xdp_aie_debug_plugin PRIVATE xdp_core metal xaiengine)
  set_target_properties(xdp_aie_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_debug_plugin
    LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
  )
endif()

install (TARGETS xdp_debug_plugin
                 xdp_appdebug_plugin
                 xdp_power_plugin
                 xdp_system_compiler_plugin
                 xdp_noc_plugin
                 xdp_vart_plugin
                 xdp_hw_emu_device_offload_plugin
                 xdp_hw_emu_pl_deadlock_plugin
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
)

# Only install these files for PCIe device for now, which is .
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
set (APPDEBUG_INSTALL_PREFIX "/opt/xilinx/xrt/share/appdebug")
install (FILES
  ${XRT_XDP_APPDEBUG_DIR}/appdebug.py
  ${XRT_XDP_APPDEBUG_DIR}/appdebugint.py
  DESTINATION ${APPDEBUG_INSTALL_PREFIX}
)
endif()

else()

# ============= Windows Specific Plugin modules ===============

endif()
