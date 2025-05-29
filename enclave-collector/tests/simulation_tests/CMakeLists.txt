cmake_minimum_required(VERSION 3.16)

# Simulation tests (only built in simulation mode)
project(simulation_tests)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../host
    ${CMAKE_CURRENT_SOURCE_DIR}/../../enclave
)

# Simulation test executable
add_executable(simulation_tests
    simulation_test.c
)

# Link libraries
target_link_libraries(simulation_tests
    common_lib
    error_codes
)

# Add test
add_test(NAME simulation_tests COMMAND simulation_tests)
