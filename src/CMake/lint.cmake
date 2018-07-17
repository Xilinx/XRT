find_program(CPPCHECK cppcheck)

include(ProcessorCount)
ProcessorCount(JOBS)
if (JOBS EQUAL 0)
   set(JOBS 1)
endif ()

if (NOT CPPCHECK)
  message (WARNING "-- cppcheck not found, C++11 code static analysis disabled --")
else ()
  add_custom_target(
    cppcheck
    COMMAND ${CPPCHECK}
    -j ${JOBS}
    -v
    -D__x86_64__
    -Dlinux
    -UCL_USE_DEPRECATED_OPENCL_1_0_APIS
    -UCL_USE_DEPRECATED_OPENCL_1_1_APIS
    --template=gcc --enable=style
    --platform=native -j 4
    --project=compile_commands.json
    )
endif ()
