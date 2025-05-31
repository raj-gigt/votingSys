#ifndef SECURE_CRYPTO_INTERFACE_H
#define SECURE_CRYPTO_INTERFACE_H

#include "shared_types.h"
#include "error_codes.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Host-side mirror of enclave crypto info structure
typedef struct {
    char election_id[128];
    uint32_t aux_count;
    bool is_complete;
    bool is_initialized;
} secure_crypto_info_t;

/**
 * Host interface to secure cryptographic operations
 * These functions either call into the enclave (in hardware mode)
 * or provide simulation implementations (in simulation mode)
 */

// Core secure crypto operations
enclave_result_t secure_crypto_init(const char* election_id, 
                                   const char* N_hex, 
                                   const char* H_hex, 
                                   const char* skA_hex);

enclave_result_t secure_process_auxiliary_value(const char* aux_hex, 
                                               const char* voter_id, 
                                               uint64_t timestamp);

enclave_result_t secure_process_auxiliary_values_batch(const char** aux_hex_values,
                                                      const char** voter_ids,
                                                      const uint64_t* timestamps,
                                                      size_t count,
                                                      int* processed_count);

enclave_result_t secure_compute_final_aggregation(char* result_hex, 
                                                 size_t* result_hex_size,
                                                 uint8_t* zk_proof, 
                                                 size_t* proof_size);

enclave_result_t secure_verify_auxiliary_value_crypto(const char* aux_hex,
                                                     const char* voter_public_key_hex,
                                                     int* is_valid);

enclave_result_t secure_crypto_cleanup(void);

// Additional secure operations
enclave_result_t secure_get_crypto_info(secure_crypto_info_t* info);
enclave_result_t secure_reset_crypto_state(void);

#ifdef __cplusplus
}
#endif

#endif // SECURE_CRYPTO_INTERFACE_H
