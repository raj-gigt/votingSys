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

// Vote processing
enclave_result_t process_vote(const vote_t* vote, vote_receipt_t* receipt);
enclave_result_t validate_vote(const vote_t* vote);
enclave_result_t aggregate_votes(vote_aggregation_t* aggregation);

// Cryptographic operations
enclave_result_t generate_keypair(crypto_key_t* public_key, crypto_key_t* private_key);
enclave_result_t sign_data(const uint8_t* data, size_t data_len, 
                          const crypto_key_t* private_key, crypto_signature_t* signature);
enclave_result_t verify_signature(const uint8_t* data, size_t data_len,
                                 const crypto_signature_t* signature, 
                                 const crypto_key_t* public_key);

// Sealed storage operations
enclave_result_t seal_data(const uint8_t* data, size_t data_len, 
                          uint8_t** sealed_data, size_t* sealed_len);
enclave_result_t unseal_data(const uint8_t* sealed_data, size_t sealed_len,
                            uint8_t** data, size_t* data_len);

// Internal helper functions
enclave_result_t compute_vote_hash(const vote_t* vote, uint8_t* hash);
enclave_result_t verify_vote_signature(const vote_t* vote);
enclave_result_t update_aggregation_state(const vote_t* vote);

#ifdef __cplusplus
}
#endif

#endif // ENCLAVE_OPERATIONS_H
