find_program(CLANGTIDY run-clang-tidy)

include(ProcessorCount)
ProcessorCount(JOBS)
if (JOBS EQUAL 0)
   set(JOBS 1)
endif ()

if (NOT CLANGTIDY)
  message (WARNING "-- run-clang-tidy not found, static code analysis disabled")
else ()
  message ("-- run-clang-tidy found, static code analysis enabled")
# run-clang-tidy uses CMake generated compile_comands.json with -p switch
  add_custom_target(
    clang-tidy
    COMMAND ${CLANGTIDY}
    -j ${JOBS}
    -p ${CMAKE_CURRENT_BINARY_DIR}
    )
endif ()
