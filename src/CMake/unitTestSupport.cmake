include(FindUnixCommands)

# Make the unit test run directory
set(UNITTEST_RUN_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/unit-tests")
execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${UNITTEST_RUN_BASE_DIR}")


#------------------------------------------------------------------------------
# Function: xrt_create_unittest_wrapper 
#
# This function will create the unit test wrapper script used to:
#    1) Encapulate the entire test in 1 bash script.
#    2) Capture and log all of the output statements from the unit test
#    3) Provide a single point of entry to execute the unit test
#
# Syntax: xrt_create_unittest_wrapper TEST_DIRECTORY TEST_COMMAND WRAPPER_FILE
#
# Where:
#   TEST_DIRECTORY - The path to the test directory
#   TEST_COMMAND   - The test command to be executed
#   WRAPPER_FILE   - The resulting wrapper file created.
#------------------------------------------------------------------------------
function(xrt_util_create_unittest_wrapper TEST_DIRECTORY TEST_COMMAND WRAPPER_FILE)
  set(RUN_BASE_NAME "runme")
  set(RUN_BASE_LOG "${RUN_BASE_NAME}.log")

  # Determine the name of the wrapper script
  if(BASH)
    set(RUN_BASE_EXECUTABLE "${RUN_BASE_NAME}.sh")
    set(RUN_BASE_WRAPPER_FILE "testBashWrapper.sh.in")
    set(SETUP_SCRIPT "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/${XRT_INSTALL_DIR}/setup.sh")
  else()
    if(WIN32)
      set(RUN_BASE_EXECUTABLE "${RUN_BASE_NAME}.bat")
      set(RUN_BASE_WRAPPER_FILE "testBatchWrapper.bat.in")
      set(SETUP_SCRIPT "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/${XRT_INSTALL_DIR}/setup.bat")
    else()
      message(FATAL_ERROR "Error: No shell found.")
    endif()
  endif()

  # Now for the fun part.
  # Since CMake doesn't support directly support creating a file with a given
  # set of permissions, we must:
  #   1) Create the file in a temp directory.
  #   2) Copy the file to a 'destination' directory and set the permissions.
  #      Note: We can simply move or copy a file directly, it has to be to a directory.
  #   3) Remove the 'temporary' directory

  # Step 1a: Create working temporary directory
  set(TEMP_DIR "${TEST_DIRECTORY}/tmp")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${TEMP_DIR}")

  # Step 1b: Create the script in the temporary directory
  configure_file(
    "${CMAKE_SOURCE_DIR}/CMake/config/testBashWrapper.sh.in"
    "${TEMP_DIR}/${RUN_BASE_EXECUTABLE}"
  )

  # Step 2: Copy the script to the test directory and changes its permissions so that
  #         it can be later executed
  file(
    COPY "${TEMP_DIR}/${RUN_BASE_EXECUTABLE}"
    DESTINATION "${TEST_DIRECTORY}"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )

  # Step 3: Clean up after ourselves
  file(REMOVE_RECURSE  "${TEMP_DIR}")

  # Return the wrapper file that was created
  set(${WRAPPER_FILE} "${TEST_DIRECTORY}/${RUN_BASE_EXECUTABLE}" PARENT_SCOPE)
endfunction()


function(xrt_util_create_unittest_dir TEST_SUITE_NAME TEST_NAME UNIT_TEST_DIR)
  # Create the test working directory
    if ("${TEST_SUITE_NAME}" STREQUAL "")
      set(${UNIT_TEST_DIR} "${UNITTEST_RUN_BASE_DIR}/${TEST_NAME}" PARENT_SCOPE)
    else()
      set(${UNIT_TEST_DIR} "${UNITTEST_RUN_BASE_DIR}/${TEST_SUITE_NAME}/${TEST_NAME}" PARENT_SCOPE)
    endif()

    # Create the test directory
    execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${UNIT_TEST_DIR}")
endfunction()

function(xrt_util_create_cmake_test_name TEST_SUITE_NAME TEST_NAME CMAKE_TEST_NAME)
  # Create the test working directory
    if ("${TEST_SUITE_NAME}" STREQUAL "")
      set(${CMAKE_TEST_NAME} "${TEST_NAME}" PARENT_SCOPE)
    else()
      set(${CMAKE_TEST_NAME} "[${TEST_SUITE_NAME}]:${TEST_NAME}" PARENT_SCOPE)
    endif()
endfunction()

function(xrt_helper_add_test TEST_SUITE_NAME TEST_NAME TEST_COMMAND)
  # Create the test working directory
  xrt_util_create_unittest_dir("${TEST_SUITE_NAME}" "${TEST_NAME}" TEST_WORKING_DIR)
  
  # Create the test wrapper script
  xrt_util_create_unittest_wrapper("${TEST_WORKING_DIR}" "${TEST_COMMAND}" WRAPPER_FILE)

  # Add the test to cmake
  xrt_util_create_cmake_test_name("${TEST_SUITE_NAME}" "${TEST_NAME}" CMAKE_TEST_NAME)
  if(BASH)
    add_test(
      NAME "${CMAKE_TEST_NAME}"
      COMMAND ${BASH} -c "${WRAPPER_FILE}"
      WORKING_DIRECTORY "${TEST_WORKING_DIR}"
      )
  else()
    if(WIN32)
      add_test(
        NAME "${CMAKE_TEST_NAME}"
        COMMAND ${CMAKE_COMMAND} -E chdir ${TEST_WORKING_DIR} $ENV{ComSpec} /c "${WRAPPER_FILE}"
        WORKING_DIRECTORY "${WRAPPER_FILE}"
      )
    else()
      message(FATAL_ERROR "Error: No shell found.")
    endif()
  endif()
endfunction()


#------------------------------------------------------------------------------
# Function: xrt_add_test 
#
#------------------------------------------------------------------------------
function(xrt_add_test TEST_NAME TEST_EXECUTABLE TEST_OPTIONS )
  # Create a test command string
  set(TEST_COMMAND "${TEST_EXECUTABLE} ${TEST_OPTIONS}" )
  xrt_helper_add_test( "${TEST_SUITE_NAME}" "${TEST_NAME}" "${TEST_COMMAND}" )
endfunction()
