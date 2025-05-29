#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "shared_types.h"
#include "error_codes.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// API client configuration
typedef struct {
    char base_url[512];
    char auth_token[256];
    int timeout_ms;
    int max_retries;
} api_config_t;

// Cryptographic parameters from external API (Paillier-specific)
typedef struct {
    char N[1024];          // N parameter as hex string
    char H[1024];          // H parameter as hex string
    char N_squared[2048];  // N^2 parameter as hex string
} crypto_params_t;

// Auxiliary value from external API
typedef struct {
    char voter_id[128];
    char aux_value[1024];  // Auxiliary value as hex string
} auxiliary_value_t;

// API client functions
enclave_result_t api_client_init(const api_config_t* config);
void api_client_cleanup(void);

// Election management
enclave_result_t api_fetch_election_params(const char* election_id, crypto_params_t* params);
enclave_result_t api_submit_auxiliary_product(const char* election_id, const char* product_hex);

// Vote data management
enclave_result_t api_fetch_auxiliary_values(const char* election_id, auxiliary_value_t** values, size_t* count);
enclave_result_t api_submit_vote_result(const char* election_id, const vote_receipt_t* receipt);

// Key management (stored externally) - Remove crypto_key_t references for now
// enclave_result_t api_store_enclave_key(const char* key_id, const crypto_key_t* key);
// enclave_result_t api_fetch_enclave_key(const char* key_id, crypto_key_t* key);

// Result storage
enclave_result_t api_store_aggregation_result(const char* election_id, const vote_aggregation_t* result);
enclave_result_t api_fetch_aggregation_result(const char* election_id, vote_aggregation_t* result);

// Additional API functions for external integration
enclave_result_t api_get_election_parameters(election_params_t* params);
enclave_result_t api_get_auxiliary_values(auxiliary_values_t* aux_values);
enclave_result_t api_store_vote_receipt(const vote_receipt_t* receipt);
enclave_result_t api_store_final_results(const final_results_t* results);
enclave_result_t api_get_keys(key_pair_t* keys);
enclave_result_t api_store_keys(const key_pair_t* keys);

// Utility functions
void api_free_auxiliary_values(auxiliary_value_t* values, size_t count);

#ifdef __cplusplus
}
#endif

#endif // API_CLIENT_H
