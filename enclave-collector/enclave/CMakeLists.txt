# Enclave CMakeLists.txt

# Set minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Check if we're building for simulation or hardware
if(SIMULATION_MODE)
    message(STATUS "Building enclave in simulation mode")
    set(ENCLAVE_TARGET "collector_enclave_sim")    set(ENCLAVE_SOURCES
        enclave_sim.c
        collector_operations.c
        crypto_operations.c
        vote_processing.c
        sealed_storage.c
        secure_math.c
    )
    set(ENCLAVE_LIBS ${BIGINT_LIBRARY})
else()
    message(STATUS "Building enclave for SGX hardware")
    find_package(OpenEnclave CONFIG REQUIRED)
    set(ENCLAVE_TARGET "collector_enclave")    set(ENCLAVE_SOURCES
        enclave.c
        collector_operations.c
        crypto_operations.c
        vote_processing.c
        sealed_storage.c
        secure_math.c
    )
    set(ENCLAVE_LIBS openenclave::oeenclave ${BIGINT_LIBRARY})
endif()

# Create enclave target
add_library(${ENCLAVE_TARGET} SHARED ${ENCLAVE_SOURCES})

# Include directories
target_include_directories(${ENCLAVE_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/edl
    ${BIGINT_INCLUDE_DIR}
    ${DEPS_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${ENCLAVE_TARGET} 
    ${ENCLAVE_LIBS}
    common_lib
)

# Compiler flags
target_compile_options(${ENCLAVE_TARGET} PRIVATE
    -Wall -Wextra -Werror
    -fstack-protector-strong
    -fPIC
)

if(NOT SIMULATION_MODE)
    # Generate enclave files for hardware SGX
    add_custom_command(OUTPUT collector_u.h collector_u.c collector_args.h
        DEPENDS ${CMAKE_SOURCE_DIR}/edl/collector.edl
        COMMAND openenclave::oeedger8r --untrusted ${CMAKE_SOURCE_DIR}/edl/collector.edl --search-path ${OE_INCLUDEDIR}/openenclave/edl/sgx
    )
    
    add_custom_command(OUTPUT collector_t.h collector_t.c
        DEPENDS ${CMAKE_SOURCE_DIR}/edl/collector.edl
        COMMAND openenclave::oeedger8r --trusted ${CMAKE_SOURCE_DIR}/edl/collector.edl --search-path ${OE_INCLUDEDIR}/openenclave/edl/sgx
    )
    
    add_custom_target(enclave_generated_files DEPENDS collector_t.h collector_t.c)
    add_dependencies(${ENCLAVE_TARGET} enclave_generated_files)
    
    target_sources(${ENCLAVE_TARGET} PRIVATE collector_t.c)
    
    # Sign the enclave
    add_custom_command(TARGET ${ENCLAVE_TARGET} POST_BUILD
        COMMAND openenclave::oesign sign -e $<TARGET_FILE:${ENCLAVE_TARGET}> -c ${CMAKE_CURRENT_SOURCE_DIR}/enclave.conf -k ${CMAKE_CURRENT_SOURCE_DIR}/private.pem
    )
endif()

# Set output directory
set_target_properties(${ENCLAVE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
