#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include "shared_types.h"
#include "error_codes.h"

#ifdef _WIN32
    #include <windows.h>
    #define THREAD_HANDLE HANDLE
#else
    #include <pthread.h>
    #define THREAD_HANDLE pthread_t
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Network configuration
typedef struct {
    char host[256];
    int port;
    int max_connections;
    int timeout_seconds;
    bool enable_ssl;
} network_config_t;

// Network server state
typedef struct {
    int server_socket;
    bool is_running;
    network_config_t config;
    THREAD_HANDLE server_thread;
} network_server_t;

// HTTP request structure
typedef struct {
    char method[16];
    char path[256];
    char* body;
    size_t body_length;
    char headers[1024];
} http_request_t;

// HTTP response structure
typedef struct {
    int status_code;
    char* body;
    size_t body_length;
    char headers[1024];
} http_response_t;

// Network API functions
enclave_result_t network_initialize(const network_config_t* config, network_server_t* server);
enclave_result_t network_start_server(network_server_t* server);
enclave_result_t network_stop_server(network_server_t* server);
enclave_result_t network_cleanup(network_server_t* server);

// HTTP handlers
enclave_result_t handle_vote_submission(const http_request_t* request, http_response_t* response);
enclave_result_t handle_vote_aggregation(const http_request_t* request, http_response_t* response);
enclave_result_t handle_enclave_info(const http_request_t* request, http_response_t* response);
enclave_result_t handle_health_check(const http_request_t* request, http_response_t* response);

// Utility functions
enclave_result_t parse_http_request(const char* raw_request, http_request_t* request);
enclave_result_t build_http_response(const http_response_t* response, char** raw_response);
void free_http_request(http_request_t* request);
void free_http_response(http_response_t* response);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_INTERFACE_H
