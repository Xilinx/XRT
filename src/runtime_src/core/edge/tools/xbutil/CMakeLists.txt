include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XRT_EDGE_TOOLS_XBUTIL_FILES
  "*.h"
  "*.cpp"
  )

add_executable(xbutil ${XRT_EDGE_TOOLS_XBUTIL_FILES})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xbutil
    xrt_core_static
    xrt_coreutil_static
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    pthread
    rt
    uuid
    ${CURSES_LIBRARIES}
    metal
    xaiengine
    )
else()
  target_link_libraries(xbutil
    xrt_core_static
    xrt_coreutil_static
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    pthread
    rt
    uuid
    ${CURSES_LIBRARIES}
    )
endif()

install (TARGETS xbutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
