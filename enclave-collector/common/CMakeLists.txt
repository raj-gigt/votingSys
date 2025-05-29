cmake_minimum_required(VERSION 3.16)

# Common library for shared components
add_library(common_lib INTERFACE)

target_include_directories(common_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create error code implementation
add_library(error_codes STATIC error_codes.c)
target_include_directories(error_codes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(common_lib INTERFACE error_codes)

# Install headers for other components to use
install(FILES
    shared_types.h
    error_codes.h
    constants.h
    DESTINATION include/common
)
