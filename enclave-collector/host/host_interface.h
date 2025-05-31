#ifndef HOST_INTERFACE_H
#define HOST_INTERFACE_H
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "shared_types.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Host configuration structure for auxiliary aggregation
typedef struct {
    int port;
    int log_level;
    int max_connections;
    int network_timeout;
    int simulation_mode;
    char config_file[256];
    char log_file[256];
    char enclave_path[256];
    char attestation_url[512];
    char backend_url[512];
    int polling_interval;
    int timeout_seconds;
} host_config_t;

// Host context structure for auxiliary operations
typedef struct {
    host_config_t config;
    void* enclave_handle;
    auxiliary_state_t aux_state;
    uint64_t session_id;
    int is_initialized;
    void* network_context;
    void* thread_pool;
    char current_collector_id[128];
    void* math_context;
    void* api_config;
} host_context_t;

// Host auxiliary state tracking
typedef struct {
    bool initialized;
    size_t processed_count;
    time_t session_start;
    time_t last_aggregation;
} host_auxiliary_state_t;

// Auxiliary request types
typedef enum {
    AUX_REQ_AGGREGATE = 1,
    AUX_REQ_STATUS = 2
} auxiliary_request_type_t;

// Auxiliary request structure
typedef struct {
    uint32_t request_id;
    auxiliary_request_type_t type;
    auxiliary_value_t* aux_values;
    size_t count;
    uint64_t timestamp;
} auxiliary_request_t;

// Auxiliary response structure
typedef struct {
    uint32_t request_id;
    enclave_result_t result;
    char result_data[512];
    size_t data_size;
    uint64_t timestamp;
} auxiliary_response_t;

// Core host interface functions for auxiliary aggregation
enclave_result_t host_initialize(host_context_t* context);
enclave_result_t host_cleanup(host_context_t* context);

// Auxiliary processing functions
enclave_result_t host_process_auxiliary_request(host_context_t* context, 
                                              const auxiliary_request_t* request,
                                              auxiliary_response_t* response);
enclave_result_t host_handle_auxiliary_aggregation(host_context_t* context,
                                                 const auxiliary_value_t* aux_values,
                                                 size_t count,
                                                 char* result_buffer);
enclave_result_t host_get_auxiliary_status(host_context_t* context, char* status_buffer);

// Configuration functions
enclave_result_t host_get_config(host_context_t* context, host_config_t* config);
enclave_result_t host_set_config(host_context_t* context, const host_config_t* config);

// Utility functions
uint64_t get_timestamp_ms(void);
int hex_to_binary(const char* hex_str, uint8_t** out_binary, size_t* out_length);

#ifdef __cplusplus
}
#endif

#endif // HOST_INTERFACE_H
