#ifndef HOST_INTERFACE_H
#define HOST_INTERFACE_H

#include <stdint.h>
#include <stddef.h>
#include "shared_types.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Host configuration structure
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
} host_config_t;

// Host context structure
typedef struct {
    host_config_t config;
    void* enclave_handle;
    collector_state_t collector_state;
    uint64_t session_id;
    int is_initialized;
    void* network_context;
    void* thread_pool;
    char current_election_id[128];  // Current election being processed
    void* math_context;             // Pointer to math_context_t (opaque to avoid header deps)
    void* api_config;               // Pointer to api_config_t
} host_context_t;

// Request structure for processing
typedef struct {
    uint32_t request_id;
    uint32_t message_type;
    uint8_t* payload;
    size_t payload_size;
    void* client_context;
    uint64_t timestamp;
} host_request_t;

// Response structure
typedef struct {
    uint32_t request_id;
    int result_code;
    uint8_t* response_data;
    size_t response_size;
} host_response_t;

// Host interface functions
// Enclave management
int host_create_enclave(host_context_t* context);
int host_destroy_enclave(host_context_t* context);
int host_initialize_enclave(host_context_t* context);

// Request processing
int host_process_request(host_context_t* context, const host_request_t* request, host_response_t* response);
int host_process_pending_requests(host_context_t* context);
int host_handle_vote_submission(host_context_t* context, const vote_t* vote, uint32_t* vote_id);
int host_handle_aggregation_request(host_context_t* context, aggregation_summary_t* summary);
int host_handle_status_request(host_context_t* context, collector_state_t* state);

// Utility functions
int host_validate_request(const host_request_t* request);
void host_perform_maintenance(host_context_t* context);
uint64_t get_timestamp_ms(void);

// New enclave_result_t API functions for integration tests
enclave_result_t host_initialize(host_context_t* context);
enclave_result_t host_cleanup(host_context_t* context);
enclave_result_t host_get_enclave_info(enclave_info_t* info);
enclave_result_t host_generate_keypair(crypto_key_t* public_key, crypto_key_t* private_key);
enclave_result_t host_process_vote_request(const vote_t* vote, vote_receipt_t* receipt);
enclave_result_t host_get_vote_aggregation(vote_aggregation_t* aggregation);

// Additional integration test functions
enclave_result_t host_process_vote(const vote_t* vote, vote_receipt_t* receipt);
enclave_result_t host_aggregate_votes(vote_aggregation_t* aggregation);

// Simulation mode functions
#ifdef SIMULATION_MODE
int sim_initialize_collector(collector_state_t* state);
int sim_process_vote(const vote_t* vote, uint32_t* vote_id);
int sim_aggregate_votes(aggregation_summary_t* summary);
int sim_generate_keys(key_pair_t* keys);
int sim_encrypt_data(const uint8_t* plaintext, size_t data_size, encrypted_data_t* encrypted);
int sim_decrypt_data(const encrypted_data_t* encrypted, uint8_t* plaintext, size_t* buffer_size);
int sim_sign_data(const uint8_t* data, size_t data_size, uint8_t* signature, size_t* sig_size);
int sim_verify_signature(const uint8_t* data, size_t data_size, const uint8_t* signature, size_t sig_size, const uint8_t* public_key, size_t key_size, int* is_valid);
#endif

#ifdef __cplusplus
}
#endif

#endif // HOST_INTERFACE_H
