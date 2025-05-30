cmake_minimum_required(VERSION 3.16)

# Host application
add_executable(collector_host
    host_main.c
    host_interface.c
    file_operations.c
    network_interface.c
    http_handlers.c
    host_callbacks.c
    logging.c
    api_client.c
    election_management.c
    config_manager.c
)

# Include directories
target_include_directories(collector_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../edl
)

# Link common libraries
target_link_libraries(collector_host PRIVATE
    common_lib
    error_codes
)

# Add dependencies
if(TARGET bigint_lib)
    target_link_libraries(collector_host PRIVATE bigint_lib)
endif()

if(TARGET tommath)
    target_link_libraries(collector_host PRIVATE tommath)
elseif(TARGET libtommath)
    target_link_libraries(collector_host PRIVATE libtommath)
endif()

if(TARGET libcurl)
    target_link_libraries(collector_host PRIVATE libcurl)
    if(WIN32)
        target_link_libraries(collector_host PRIVATE ws2_32 wldap32 crypt32 normaliz)
        target_compile_definitions(collector_host PRIVATE CURL_STATICLIB)
    endif()
elseif(TARGET CURL::libcurl)
    target_link_libraries(collector_host PRIVATE CURL::libcurl)
    if(WIN32)
        target_link_libraries(collector_host PRIVATE ws2_32 wldap32 crypt32 normaliz)
        target_compile_definitions(collector_host PRIVATE CURL_STATICLIB)
    endif()
endif()

# Open Enclave SDK for production builds
if(NOT SIMULATION_MODE AND TARGET openenclave::oehost)
    target_link_libraries(collector_host PRIVATE openenclave::oehost)
    target_compile_definitions(collector_host PRIVATE OE_BUILD_ENCLAVE)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(collector_host PRIVATE ws2_32 crypt32 bcrypt)
    target_compile_definitions(collector_host PRIVATE 
        WIN32 
        _WINDOWS 
        _WIN32_WINNT=0x0600
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_link_libraries(collector_host PRIVATE pthread m)
endif()

# Set output directory
set_target_properties(collector_host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
