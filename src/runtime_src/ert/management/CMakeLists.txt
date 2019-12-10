add_custom_command(
 OUTPUT bsp.extracted
 COMMAND ${CMAKE_COMMAND} -E make_directory bsp
 COMMAND tar -C bsp -jxf ${CMAKE_CURRENT_SOURCE_DIR}/mgmt_bsp.tar.bz2
 COMMAND touch bsp.extracted
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mgmt_bsp.tar.bz2
 COMMENT "Extracting board support packet for ERT management"
)

add_custom_command(
 OUTPUT mgmt.bin
 COMMAND $(MAKE) BLDDIR=${CMAKE_CURRENT_BINARY_DIR} SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/management.mk ert
 DEPENDS bsp.extracted ${CMAKE_CURRENT_SOURCE_DIR}/board_mon.c
 COMMENT "Generating binary for ERT management"
)

add_custom_target(management
 DEPENDS mgmt.bin
)

install (FILES
 ${CMAKE_CURRENT_BINARY_DIR}/mgmt.bin
 DESTINATION ${ERT_INSTALL_PREFIX}
)
