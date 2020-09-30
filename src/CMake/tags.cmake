add_custom_target(
  tags
  COMMAND ${CTAGS}
  --root ${CMAKE_SOURCE_DIR}
  --etags
  -f TAGS
  )
