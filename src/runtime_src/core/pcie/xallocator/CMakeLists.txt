include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  )

file(GLOB XALLOCATOR_FILES
  "*.h"
  "*.cpp"
  )

add_library(xallocatorobj OBJECT ${XALLOCATOR_FILES})

set(ALLOCATOR_SRC "")
add_library(xallocator SHARED ${ALLOCATOR_SRC}
  $<TARGET_OBJECTS:xallocatorobj>
)

set_target_properties(xallocator PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

install (TARGETS xallocator LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
