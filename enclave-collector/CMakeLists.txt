cmake_minimum_required(VERSION 3.16)
project(EnclaveCollector VERSION 1.0.0 LANGUAGES C CXX)

# Set C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Global compiler flags
if(WIN32)
    add_compile_definitions(WIN32 _WINDOWS _WIN32_WINNT=0x0600)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /O2 /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /O2 /D_CRT_SECURE_NO_WARNINGS")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -fstack-protector-all")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -fstack-protector-all")
endif()

# Configuration options
option(SIMULATION_MODE "Build for simulation mode (WSL/Development)" ON)
option(ENABLE_LOGGING "Enable logging functionality" ON)
option(ENABLE_ATTESTATION "Enable remote attestation" OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add external dependencies from the deps folder
set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps")
if(EXISTS "${DEPS_DIR}/CMakeLists.txt")
    message(STATUS "Adding dependencies from ${DEPS_DIR}")
    add_subdirectory(${DEPS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/deps)
else()
    message(WARNING "Dependencies directory not found at ${DEPS_DIR}")
endif()

# Add common bigint library
set(BIGINT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/bigint_lib")
if(EXISTS "${BIGINT_LIB_DIR}/CMakeLists.txt")
    message(STATUS "Adding bigint library from ${BIGINT_LIB_DIR}")
    add_subdirectory(${BIGINT_LIB_DIR} ${CMAKE_CURRENT_BINARY_DIR}/bigint_lib)
elseif(NOT TARGET bigint_lib)
    message(STATUS "Creating bigint_lib target from collector sources")
    add_library(bigint_lib STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../collector/bigint_ops.c
    )
    target_include_directories(bigint_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../collector
    )
    if(TARGET tommath)
        target_link_libraries(bigint_lib PUBLIC tommath)
    elseif(TARGET libtommath)
        target_link_libraries(bigint_lib PUBLIC libtommath)
    endif()
endif()

# Find Open Enclave SDK for production builds
if(NOT SIMULATION_MODE)
    find_package(OpenEnclave CONFIG REQUIRED)
    if(NOT TARGET openenclave::oehost)
        message(FATAL_ERROR "Open Enclave SDK not found. Please install OE SDK or enable SIMULATION_MODE")
    endif()
else()
    message(STATUS "Building in simulation mode - Open Enclave SDK not required")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/edl
)

# Subdirectories
add_subdirectory(common)
add_subdirectory(host)

if(NOT SIMULATION_MODE)
    add_subdirectory(enclave)
endif()

# Global compile definitions
if(SIMULATION_MODE)
    add_compile_definitions(SIMULATION_MODE=1)
endif()

if(ENABLE_LOGGING)
    add_compile_definitions(ENABLE_LOGGING=1)
endif()

if(ENABLE_ATTESTATION AND NOT SIMULATION_MODE)
    add_compile_definitions(ENABLE_ATTESTATION=1)
endif()

# Print configuration summary
message(STATUS "=== Enclave Collector Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Simulation mode: ${SIMULATION_MODE}")
message(STATUS "Enable logging: ${ENABLE_LOGGING}")
message(STATUS "Enable attestation: ${ENABLE_ATTESTATION}")
message(STATUS "Dependencies found: ${DEPS_DIR}")
message(STATUS "========================================")
