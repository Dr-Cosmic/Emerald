# run_test.cmake - Runs one Emerald test program and diffs its output
# against the matching .expected file. Used by CTest.
execute_process(
    COMMAND ${EMERALD_BIN} ${TEST_FILE}
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE errout
    RESULT_VARIABLE code
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Test exited with code ${code}\nstderr:\n${errout}")
endif()
file(READ ${EXPECTED_FILE} expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Output mismatch.\n--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()
