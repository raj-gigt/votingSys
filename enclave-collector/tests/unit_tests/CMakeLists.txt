# Unit Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Find or download a testing framework (we'll use a simple custom one for now)
set(TEST_SOURCES
    test_main.c
    test_vote_processing.c
    test_crypto_operations.c
    test_network_interface.c
    test_file_operations.c
)

# Create test executable
add_executable(unit_tests ${TEST_SOURCES})

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/host
    ${CMAKE_SOURCE_DIR}/enclave
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(unit_tests PRIVATE
    common_lib
    error_codes
)

# Link host libraries for testing
target_link_libraries(unit_tests PRIVATE
    ${CMAKE_BINARY_DIR}/lib/libcollector_host_lib.a
)

if(SIMULATION_MODE)
    target_link_libraries(unit_tests PRIVATE collector_enclave_sim)
    target_compile_definitions(unit_tests PRIVATE SIMULATION_MODE=1)
endif()

# Add test target
add_custom_target(run_unit_tests
    COMMAND unit_tests
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler flags
target_compile_options(unit_tests PRIVATE
    -Wall -Wextra -Werror
    -DUNIT_TESTING=1
)
