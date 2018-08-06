find_program(COVBUILD /tools/batonroot/coverity/8.7.0/cov-analysis-8.7.0/bin/cov-build)


if (NOT COVBUILD)
  message (WARNING "-- coverity not found")
else ()

  set(COVBIN /tools/batonroot/coverity/8.7.0/cov-analysis-8.7.0/bin)
  set(COVSRCDIR "${CMAKE_CURRENT_SOURCE_DIR}")

  option(COVUSER "Specify Coverity User" admin)
  option(COVPW "Specify Coverity Password" invalid)

  add_custom_target(
    covconfig-c++
    COMMAND ${COVBIN}/cov-configure
    --template --config config/conf.xml --compiler c++
    )

  add_custom_target(
    covconfig-gcc
    COMMAND ${COVBIN}/cov-configure
    --template --config config/conf.xml --compiler gcc
    )

  add_custom_target(
    covbuild-user
    DEPENDS covconfig-c++ covconfig-gcc
    COMMAND ${COVBIN}/cov-build
    --config config/conf.xml
    --dir imed
    make VERBOSE=1 -j4
    )

  add_custom_target(
    covanalyze
    DEPENDS covbuild-user
    COMMAND ${COVBIN}/cov-analyze
    --all --dir imed --strip-path ${COVSRCDIR} --config config/conf.xml
    --enable-constraint-fpp
    --enable-callgraph-metrics
    --enable-exceptions
    --enable-fnptr
    --disable-default
    --enable ARRAY_VS_SINGLETON
    --enable ASSERT_SIDE_EFFECT
    --enable ATOMICITY
    --enable BAD_ALLOC_ARITHMETIC
    --enable BAD_ALLOC_STRLEN
    --enable BAD_COMPARE
    --enable BAD_FREE
    --enable BAD_OVERRIDE
    --enable BAD_SIZEOF
    --enable BUFFER_SIZE
    --enable CHAR_IO
    --enable CHECKED_RETURN
    --enable COPY_PASTE_ERROR
    --enable CONSTANT_EXPRESSION_RESULT
    --enable CTOR_DTOR_LEAK
    --enable DELETE_ARRAY
    --enable DELETE_VOID
    --enable DIVIDE_BY_ZERO
    --enable ENUM_AS_BOOLEAN
    --enable EVALUATION_ORDER
    --enable INCOMPATIBLE_CAST
    --enable INFINITE_LOOP
    --enable INTEGER_OVERFLOW
    --enable INVALIDATE_ITERATOR
    --enable LOCK
    --enable MISMATCHED_ITERATOR
    --enable MISSING_BREAK
    --enable MISSING_LOCK
    --enable MISSING_RETURN
    --enable NEGATIVE_RETURNS
    --enable NESTING_INDENT_MISMATCH
    --enable NO_EFFECT
    --enable NULL_RETURNS
    --enable OPEN_ARGS
    --enable ORDER_REVERSAL
    --enable OVERFLOW_BEFORE_WIDEN
    --enable OVERRUN
    --enable PASS_BY_VALUE
    --enable RESOURCE_LEAK
    --enable RETURN_LOCAL
    --enable REVERSE_NEGATIVE
    --enable SIZEOF_MISMATCH
    --enable SLEEP
    --enable STRAY_SEMICOLON
    --enable STRING_NULL
    --enable STRING_OVERFLOW
    --enable STRING_SIZE
    --enable UNINIT
    --enable UNINIT_CTOR
    --enable UNREACHABLE
    --enable USE_AFTER_FREE
    --enable VIRTUAL_DTOR
    --enable WRAPPER_ESCAPE
    --checker-option CONSTANT_EXPRESSION_RESULT:report_constant_logical_operands:true
    )

  add_custom_target(
    covcommit
    DEPENDS covanalyze
    COMMAND ${COVBIN}/cov-commit-defects
    --dir imed
    --host xcocoverity04
    --port 8081
    --user $(COVUSER)
    --password $(COVPW)
    --stream XRT
    --scm git
    --description \"$(DATE)\"
    )

  add_custom_target(
    coverity
    DEPENDS covcommit
    )
endif ()
