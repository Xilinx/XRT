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
set(XRT_XDP_PROFILE_COLLECTION_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/collection")
set(XRT_XDP_PROFILE_WRITER_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer")
set(XRT_XDP_PROFILE_HAL_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/hal")
set(XRT_XDP_PROFILE_CORE_DIR           "${CMAKE_CURRENT_SOURCE_DIR}/profile/core")
set(XRT_XDP_PROFILE_DEVICE_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/device")
set(XRT_XDP_PROFILE_HAL_DEVICE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/device/hal_device")
set(XRT_XDP_PROFILE_LOP_PLUGIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/lop")
set(XRT_XDP_PROFILE_LOP_WRITER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/lop")

# Files for the new xdp core
set(XRT_XDP_PROFILE_BASE_PLUGIN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/profile/plugin/vp_base")
set(XRT_XDP_PROFILE_DATABASE_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/profile/database")
set(XRT_XDP_PROFILE_EVENTS_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/profile/database/events")
set(XRT_XDP_PROFILE_VP_WRITER_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/profile/writer/vp_base")

file(GLOB XRT_NEW_XDP_CORE_FILES
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_BASE_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DATABASE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.h"
  "${XRT_XDP_PROFILE_EVENTS_DIR}/*.cpp"
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

file(GLOB XRT_XDP_PROFILE_LOP_PLUGIN_FILES
  "${XRT_XDP_PROFILE_LOP_PLUGIN_DIR}/*.h"
  "${XRT_XDP_PROFILE_LOP_PLUGIN_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_LOP_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_LOP_WRITER_DIR}/*.cpp"
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
add_library(xdp_hal_plugin SHARED ${XRT_XDP_PROFILE_HAL_PLUGIN_FILES})
add_library(xdp_hal_api_interface_plugin SHARED ${XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES})
add_library(xdp_debug_plugin SHARED ${XRT_XDP_DEBUG_FILES})
add_library(xdp_appdebug_plugin SHARED ${XRT_XDP_APPDEBUG_FILES})
add_library(xdp_lop_plugin SHARED ${XRT_XDP_PROFILE_LOP_PLUGIN_FILES})

add_dependencies(xdp xrt_coreutil)
add_dependencies(oclxdp xilinxopencl xdp)
add_dependencies(xdp_core xrt_coreutil)
add_dependencies(xdp_hal_plugin xdp_core)
add_dependencies(xdp_hal_api_interface_plugin xdp)
add_dependencies(xdp_debug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
add_dependencies(xdp_lop_plugin xdp_core)

target_link_libraries(xdp xrt_coreutil)
target_link_libraries(oclxdp xilinxopencl xdp)
target_link_libraries(xdp_core xrt_coreutil)
target_link_libraries(xdp_hal_plugin xdp_core)
target_link_libraries(xdp_hal_api_interface_plugin xdp)
target_link_libraries(xdp_debug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_appdebug_plugin xrt_coreutil xilinxopencl)
target_link_libraries(xdp_lop_plugin xdp_core)

# ====== Linux Specifics =======

set_target_properties(xdp_core PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(oclxdp PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_hal_api_interface_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_debug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_appdebug_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})
set_target_properties(xdp_lop_plugin PROPERTIES VERSION ${XRT_VERSION_STRING} SOVERSION ${XRT_SOVERSION})

install (TARGETS xdp oclxdp xdp_core xdp_hal_plugin xdp_hal_api_interface_plugin xdp_debug_plugin xdp_appdebug_plugin xdp_lop_plugin LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

install (FILES "${XRT_XDP_PROFILE_XMA_PLUGIN_DIR}/xma_profile.h" DESTINATION ${XRT_INSTALL_INCLUDE_DIR})

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
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.h"
  "${XRT_XDP_PROFILE_DEVICE_DIR}/*.cpp"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.h"
  "${XRT_XDP_PROFILE_VP_WRITER_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

add_library(xdp SHARED ${XRT_XDP_ALL_SRC_WIN})
add_library(oclxdp SHARED ${XRT_XDP_PROFILE_OCL_PLUGIN_FILES})
add_library(xdp_hal_plugin SHARED ${XRT_XDP_PROFILE_HAL_PLUGIN_FILES})
add_library(xdp_hal_api_interface_plugin SHARED ${XRT_XDP_PROFILE_HAL_INTERFACE_PLUGIN_FILES})

add_dependencies(xdp xrt_core xilinxopencl)
add_dependencies(oclxdp xdp xilinxopencl)
add_dependencies(xdp_hal_plugin xdp xrt_core)
add_dependencies(xdp_hal_api_interface_plugin xdp)

target_link_libraries(xdp xrt_core xilinxopencl)
target_link_libraries(oclxdp xdp xilinxopencl)
target_link_libraries(xdp_hal_plugin xdp xrt_core)
target_link_libraries(xdp_hal_api_interface_plugin xdp)

install(TARGETS xdp oclxdp xdp_hal_plugin xdp_hal_api_interface_plugin
  ARCHIVE DESTINATION ${XRT_INSTALL_DIR}/lib
  LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib
  RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)

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
