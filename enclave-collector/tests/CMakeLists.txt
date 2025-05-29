cmake_minimum_required(VERSION 3.16)

# Test configuration
enable_testing()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../host
)

# Test libraries
set(TEST_LIBRARIES
    common_lib
    error_codes
)

# Unit tests
add_subdirectory(unit_tests)

# Add test discovery
add_test(NAME unit_tests COMMAND unit_tests)

# Integration tests
add_subdirectory(integration_tests)

# Simulation tests (only in simulation mode)
if(SIMULATION_MODE)
    add_subdirectory(simulation_tests)
endif()

# Test data and scripts
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_data/sample_vote.json
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/sample_vote.json
    COPYONLY
)

# Custom test target
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests
    COMMENT "Running all tests"
)
