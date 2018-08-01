add_custom_target(
  tags
  COMMAND ${CTAGS}
  --project compile_commands.json
  --etags
  -f TAGS
  )
