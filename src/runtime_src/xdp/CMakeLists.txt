if (NOT WIN32)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${XRT_SRC_DIR}/include/1_2
  )

else()
# = WINDOWS ===================================================================

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${XRT_SRC_DIR}/include/1_2
  ${KHRONOS}/include
  )
endif()

# ====== File directory definitions for both Linux and Windows =========

set(XRT_XDP_APPDEBUG_DIR               "${CMAKE_CURRENT_SOURCE_DIR}/appdebug")
set(XRT_XDP_DEBUG_DIR                  "${CMAKE_CURRENT_SOURCE_DIR}/debug")
set(XRT_XDP_PROFILE_DIR                "${CMAKE_CURRENT_SOURCE_DIR}/profile")
set(XRT_XDP_PROFILE_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin")
set(XRT_XDP_PROFILE_XMA_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/xma")
set(XRT_XDP_PROFILE_OCL_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/ocl")
set(XRT_XDP_PROFILE_HAL_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/hal")
set(XRT_XDP_PROFILE_HAL_API_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/hal_api_interface")
set(XRT_XDP_PROFILE_AIE_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie")
set(XRT_XDP_PROFILE_AIE_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/aie")
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
set(XRT_XDP_PROFILE_USER_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/user")
set(XRT_XDP_PROFILE_USER_WRITER_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/user")
set(XRT_XDP_PROFILE_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload")
set(XRT_XDP_PROFILE_DEVICE_OFFLOAD_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/device_trace")
set(XRT_XDP_PROFILE_OPENCL_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload/opencl")
set(XRT_XDP_PROFILE_HAL_DEVICE_OFFLOAD_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/device_offload/hal")
set(XRT_XDP_PROFILE_DEVICE_AIE_TRACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/aie_trace")
set(XRT_XDP_AIE_TRACE_PLUGIN_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/aie_trace")
set(XRT_XDP_AIE_TRACE_WRITER_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/aie_trace")
set(XRT_XDP_POWER_PROFILE_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/power")
set(XRT_XDP_POWER_PROFILE_WRITER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/power")

# Files for the new xdp core
set(XRT_XDP_PROFILE_BASE_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/vp_base")
set(XRT_XDP_PROFILE_DATABASE_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/profile/database")
set(XRT_XDP_PROFILE_EVENTS_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/database/events")
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

file(GLOB XRT_XDP_PROFILE_XMA_PLUGIN_FILES
  "${XRT_XDP_PROFILE_XMA_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_XMA_PLUGIN_DIR}/*.cpp"
  )

file(GLOB XRT_XDP_PROFILE_HAL_PLUGIN_FILES
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_HAL_DEVICE_DIR}/*.cpp"
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

file(GLOB XRT_XDP_POWER_PROFILE_PLUGIN_FILES
  "${XRT_XDP_POWER_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_POWER_PROFILE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_POWER_PROFILE_WRITER_DIR}/*.h"
  "${XRT_XDP_POWER_PROFILE_WRITER_DIR}/*.cpp"
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

file(GLOB XRT_XDP_PROFILE_VART_PLUGIN_FILES
  "${XRT_XDP_PROFILE_VART_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_VART_PLUGIN_DIR}/*.cpp"
  )
  
set(XRT_XDP_ALL_SRC
  ${XRT_XDP_PROFILE_FILES}
  ${XRT_XDP_FILES}
  )

if (NOT WIN32)

# ===== XDP Libraries on Linux =======
add_library(xdp_core SHARED ${XRT_NEW_XDP_CORE_FILES})
add_library(xdp SHARED ${XRT_XDP_ALL_SRC})
add_library(oclxdp SHARED ${XRT_XDP_PROFILE_OCL_PLUGIN_FILES})

# Plugin modules
add_library(xdp_hal_plugin MODULE ${XRT_XDP_PROFILE_HAL_PLUGIN_FILES})
add_library(xdp_hal_api_interface_plugin MODULE ${XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES})
add_library(xdp_debug_plugin MODULE ${XRT_XDP_DEBUG_FILES})
add_library(xdp_appdebug_plugin MODULE ${XRT_XDP_APPDEBUG_FILES})
add_library(xdp_lop_plugin MODULE ${XRT_XDP_PROFILE_LOP_PLUGIN_FILES})
add_library(xdp_user_plugin MODULE ${XRT_XDP_PROFILE_USER_PLUGIN_FILES})
add_library(xdp_device_offload_plugin MODULE ${XRT_XDP_OPENCL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES})
add_library(xdp_hal_device_offload_plugin MODULE ${XRT_XDP_HAL_PROFILE_DEVICE_OFFLOAD_PLUGIN_FILES})
add_library(xdp_power_plugin MODULE ${XRT_XDP_POWER_PROFILE_PLUGIN_FILES})
add_library(xdp_noc_plugin MODULE ${XRT_XDP_PROFILE_NOC_PLUGIN_FILES})
add_library(xdp_vart_plugin MODULE ${XRT_XDP_PROFILE_VART_PLUGIN_FILES})

add_dependencies(xdp xrt_coreutil)
add_dependencies(oclxdp xilinxopencl xdp)
add_dependencies(xdp_core xrt_coreutil)
add_dependencies(xdp_hal_plugin xdp_core xdp xrt_core)
add_dependencies(xdp_hal_api_interface_plugin xdp )
add_dependencies(xdp_debug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_lop_plugin xdp_core)
add_dependencies(xdp_user_plugin xdp_core)
add_dependencies(xdp_device_offload_plugin xdp_core xilinxopencl)
add_dependencies(xdp_hal_device_offload_plugin xdp_core xrt_core)
add_dependencies(xdp_power_plugin xdp_core xrt_core)
add_dependencies(xdp_noc_plugin xdp_core xrt_core)
add_dependencies(xdp_vart_plugin xdp xrt_core)

target_link_libraries(xdp xrt_coreutil)
target_link_libraries(oclxdp xilinxopencl xdp)
target_link_libraries(xdp_core xrt_coreutil)
target_link_libraries(xdp_hal_plugin xdp_core xdp xrt_core)
target_link_libraries(xdp_hal_api_interface_plugin xdp xrt_core)
target_link_libraries(xdp_debug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_lop_plugin xdp_core)
target_link_libraries(xdp_user_plugin xdp_core)
target_link_libraries(xdp_device_offload_plugin xdp_core xilinxopencl)
target_link_libraries(xdp_hal_device_offload_plugin xdp_core xrt_core)
target_link_libraries(xdp_power_plugin xdp_core xrt_core)
target_link_libraries(xdp_noc_plugin xdp_core xrt_core)
target_link_libraries(xdp_vart_plugin xdp xrt_core)

# AI Engine plugin is Edge only
if (DEFINED XRT_AIE_BUILD)
  add_library(xdp_aie_plugin MODULE ${XRT_XDP_PROFILE_AIE_PLUGIN_FILES})
  add_dependencies(xdp_aie_plugin xdp_core xrt_core)
  target_link_libraries(xdp_aie_plugin xdp_core xrt_core metal xaiengine)
  set_target_properties(xdp_aie_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

  install (TARGETS xdp_aie_plugin
    LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
  )
endif()

# Build AIE Trace plugin 
add_library(xdp_aie_trace_plugin MODULE ${XRT_XDP_AIE_TRACE_PLUGIN_FILES})
add_dependencies(xdp_aie_trace_plugin xdp_core xrt_core)

if (DEFINED XRT_AIE_BUILD)
  # GMIO flow is supported for Edge only
  target_link_libraries(xdp_aie_trace_plugin xdp_core xrt_core metal xaiengine)
else()
  target_link_libraries(xdp_aie_trace_plugin xdp_core xrt_core)
endif()

set_target_properties(xdp_aie_trace_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp_aie_trace_plugin 
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
)
# end of Build AIE Trace plugin 


# ====== Linux Specifics =======

set_target_properties(xdp_core PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(oclxdp PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_api_interface_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_noc_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_vart_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_appdebug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_lop_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_user_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_device_offload_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_power_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp oclxdp xdp_core
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install (TARGETS xdp oclxdp xdp_core
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)

install (TARGETS xdp_hal_plugin xdp_hal_api_interface_plugin xdp_debug_plugin xdp_appdebug_plugin xdp_lop_plugin xdp_user_plugin xdp_device_offload_plugin xdp_hal_device_offload_plugin xdp_power_plugin xdp_noc_plugin xdp_vart_plugin
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR}/xrt/module
)

install (FILES "${XRT_XDP_PROFILE_XMA_PLUGIN_DIR}/xma_profile.h"
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
  COMPONENT ${XRT_DEV_COMPONENT}
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
# ====== Windows Specifics =======

# Do not build debug, appdebug, or LOP for Windows

file(GLOB XRT_XDP_ALL_SRC_WIN
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OCL_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_OCL_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_COLLECTION_DIR}/*.h"
  "${XRT_XDP_PROFILE_COLLECTION_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_WRITER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_CORE_DIR}/*.h"
  "${XRT_XDP_PROFILE_CORE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.h"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_EVENTS_CREATOR_DIR}/*.h"
  "${XRT_XDP_PROFILE_EVENTS_CREATOR_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_OCL_DEVICE_LOGGER_DIR}/*.h"
  "${XRT_XDP_PROFILE_OCL_DEVICE_LOGGER_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_XRT_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

add_library(xdp SHARED ${XRT_XDP_ALL_SRC_WIN})
add_library(oclxdp SHARED ${XRT_XDP_PROFILE_OCL_PLUGIN_FILES})

# Plugin modules
add_library(xdp_hal_plugin MODULE ${XRT_XDP_PROFILE_HAL_PLUGIN_FILES})
add_library(xdp_hal_api_interface_plugin MODULE ${XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES})
add_library(xdp_vart_plugin MODULE ${XRT_XDP_PROFILE_VART_PLUGIN_FILES})

add_dependencies(xdp xrt_core xilinxopencl)
add_dependencies(oclxdp xdp xilinxopencl)
add_dependencies(xdp_hal_plugin xdp xrt_core)
add_dependencies(xdp_hal_api_interface_plugin xdp)
add_dependencies(xdp_vart_plugin xdp xrt_core)

target_link_libraries(xdp xrt_core xilinxopencl)
target_link_libraries(oclxdp xdp xilinxopencl)
target_link_libraries(xdp_hal_plugin xdp xrt_core)
target_link_libraries(xdp_hal_api_interface_plugin xdp)
target_link_libraries(xdp_vart_plugin xdp xrt_core)

install(TARGETS oclxdp xdp xdp_hal_plugin xdp_hal_api_interface_plugin xdp_vart_plugin
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS oclxdp xdp xdp_hal_plugin xdp_hal_api_interface_plugin xdp_vart_plugin
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)

# Only install these files for PCIe device for now, which is .
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
set (APPDEBUG_INSTALL_PREFIX "${XRT_INSTALL_DIR}/share/appdebug")
install (FILES
  ${XRT_XDP_APPDEBUG_DIR}/appdebug.py
  ${XRT_XDP_APPDEBUG_DIR}/appdebugint.py
  DESTINATION ${APPDEBUG_INSTALL_PREFIX}
)
endif()
endif()
