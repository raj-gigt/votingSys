#ifndef SECURE_CRYPTO_PROCESSOR_H
#define SECURE_CRYPTO_PROCESSOR_H

#include "shared_types.h"
#include "error_codes.h"
#include "../../common/bigint_lib/bigint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VOTERS 10000
#define MAX_VOTER_ID_LENGTH 64
#define ZK_PROOF_SIZE 256
#define MAX_AUX_VALUES 10000

// Forward declaration for math context
typedef struct math_context_t math_context_t;

// Structure to track processed voters (for audit)
typedef struct {
    char voter_id[MAX_VOTER_ID_LENGTH];
    uint64_t timestamp;
} processed_voter_t;

// Secure crypto state stored within enclave
typedef struct {
    char election_id[128];
      // Cryptographic parameters (stored securely in enclave)
    BigInt N;           // Public parameter N
    BigInt H;           // Generator H  
    BigInt skA;         // Aggregator secret key (highly sensitive)
    BigInt N_squared;   // N^2 for efficiency
    BigInt pk_A;        // Aggregator public key: H^sk_A mod N^2
    
    // Aggregation state
    BigInt aux_product; // Product of all auxiliary values
    uint32_t aux_count;          // Number of processed auxiliary values
    bool is_complete;            // Whether final aggregation is complete
    
    // Audit trail (stored securely)
    processed_voter_t processed_voters[MAX_AUX_VALUES];
} secure_crypto_state_t;

// Crypto state info for host queries
typedef struct {
    char election_id[128];
    uint32_t aux_count;
    bool is_complete;
    bool is_initialized;
} secure_crypto_info_t;

// Core secure crypto operations (executed within enclave)
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

// Mathematical context operations (consolidated from secure_math)
enclave_result_t secure_math_init(math_context_t* context, 
                                 const char* N_hex, 
                                 const char* H_hex, 
                                 const char* N_squared_hex);

enclave_result_t secure_math_cleanup(math_context_t* context);

enclave_result_t secure_math_process_auxiliary(math_context_t* context, 
                                              const char* aux_value_hex);

enclave_result_t secure_math_get_product(const math_context_t* context, 
                                        char* product_hex, 
                                        size_t buffer_size);

enclave_result_t secure_math_reset_product(math_context_t* context);

enclave_result_t secure_math_verify_auxiliary(const math_context_t* context,
                                             const char* aux_value_hex,
                                             const char* proof_hex);

#ifdef __cplusplus
}
#endif

#endif // SECURE_CRYPTO_PROCESSOR_H
