cmake_minimum_required(VERSION 3.16)

# Integration tests
project(integration_tests)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../host
    ${CMAKE_CURRENT_SOURCE_DIR}/../../enclave
)

# Integration test executable
add_executable(integration_tests
    integration_test.c
)

# Link libraries
target_link_libraries(integration_tests
    common_lib
    error_codes
)

# Add test
add_test(NAME integration_tests COMMAND integration_tests)
