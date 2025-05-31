#ifndef ENCLAVE_OPERATIONS_H
#define ENCLAVE_OPERATIONS_H

#include "shared_types.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Core enclave operations
enclave_result_t initialize_enclave_state(void);
enclave_result_t cleanup_enclave_state(void);
enclave_result_t get_enclave_info(enclave_info_t* info);

// Auxiliary aggregation operations
enclave_result_t process_auxiliary_values(const auxiliary_value_t* values, size_t count, 
                                        auxiliary_product_t* product);
enclave_result_t validate_auxiliary_value(const auxiliary_value_t* value);
enclave_result_t aggregate_auxiliary_values(const api_auxiliary_value_t* aux_values, 
                                          size_t count, 
                                          char* result_buffer, 
                                          size_t buffer_size);

// Cryptographic operations
enclave_result_t generate_keypair(crypto_key_t* public_key, crypto_key_t* private_key);
enclave_result_t sign_data(const uint8_t* data, size_t data_len, 
                          const crypto_key_t* private_key, crypto_signature_t* signature);
enclave_result_t verify_signature(const uint8_t* data, size_t data_len,
                                 const crypto_signature_t* signature, 
                                 const crypto_key_t* public_key);

// Cryptographic auxiliary aggregation (Paillier-based)
enclave_result_t crypto_aggregate_auxiliary_values(const char** aux_values, size_t count, 
                                                  const crypto_params_t* params, char* result);
enclave_result_t verify_auxiliary_value(const char* aux_value);

// Internal helper functions
enclave_result_t compute_auxiliary_hash(const auxiliary_value_t* value, uint8_t* hash);
enclave_result_t verify_auxiliary_signature(const auxiliary_value_t* value);
enclave_result_t update_aggregation_state(const auxiliary_value_t* value);

#ifdef __cplusplus
}
#endif

#endif // ENCLAVE_OPERATIONS_H
