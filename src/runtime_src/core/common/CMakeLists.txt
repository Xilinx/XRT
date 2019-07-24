include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )


# Files to include in shared library
file(GLOB XRT_CORECOMMON_LIB_FILES
  "config_reader.*"
  "message.*"
  "t_time.*"
  "xclbin_parser.*"
  "sensor.*"
  "utils.*"
  )

# Files to include in object list
file(GLOB XRT_CORECOMMON_OBJ_FILES
  "scheduler.*"
  )

add_library(xrt_coreutil SHARED ${XRT_CORECOMMON_LIB_FILES})
add_library(xrt_coreutil_static STATIC ${XRT_CORECOMMON_LIB_FILES})

set_target_properties(xrt_coreutil PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

# Fully resolved library
target_link_libraries(xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  uuid
  )

install (TARGETS xrt_coreutil LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)

# The scheduler object files are for auto config of scheduler. These
# files reference xrt_core symbols, hence are excluded from
# xrt_corecommon shared library and instead linked explicitly into
# client (core) libraries
add_library(core_common_objects OBJECT ${XRT_CORECOMMON_OBJ_FILES})
