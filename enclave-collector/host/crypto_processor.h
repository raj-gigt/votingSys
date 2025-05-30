#ifndef CRYPTO_PROCESSOR_H
#define CRYPTO_PROCESSOR_H

#include "shared_types.h"
#include "error_codes.h"
#include "host_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// Include the old collector module types
typedef struct {
    uint8_t* data;
    size_t length;
} BigInt;

typedef struct {
    BigInt N;        // The modulus N = p*q
    BigInt N_squared; // N^2
    BigInt H;        // The hash function output in Z_N^2*
} ElectionParams;

// Cryptographic processor context
typedef struct {
    ElectionParams election_params;
    BigInt running_product;  // Current auxiliary product: ∏(i=1 to n) aux_i
    BigInt pk_A;            // Public key A: H()^sk_A
    uint32_t aux_count;     // Number of auxiliary values processed
    bool is_initialized;
    bool is_sealed;
    host_context_t* host_context;
} crypto_processor_t;

// Auxiliary value structure for the mathematical protocol
typedef struct {
    BigInt aux_i;           // aux_i = pk_A^sk_i = H()^(sk_A * sk_i)
    uint32_t user_id;
    uint64_t timestamp;
    uint8_t signature[64];  // Cryptographic signature for verification
} auxiliary_value_t;

// Final aggregation result
typedef struct {
    BigInt final_aux;       // aux = ∏(i=1 to n) aux_i = H()^(sk_A * Σ(i=1 to n) sk_i)
    uint32_t total_users;
    uint64_t computation_time;
    uint8_t aggregation_proof[128];
} aggregation_result_t;

// Function declarations

enclave_result_t crypto_processor_init(crypto_processor_t* processor, host_context_t* host_ctx);
enclave_result_t crypto_processor_cleanup(crypto_processor_t* processor);

// Core cryptographic operations from the mathematical protocol
enclave_result_t crypto_process_auxiliary_value(crypto_processor_t* processor, const auxiliary_value_t* aux_value);
enclave_result_t crypto_compute_final_aggregation(crypto_processor_t* processor, aggregation_result_t* result);
enclave_result_t crypto_verify_auxiliary_value(const auxiliary_value_t* aux_value, const ElectionParams* params);

// Secure computation functions (executed in enclave when available)
int secure_multiply_aux_values(const BigInt* current_product, const BigInt* new_aux, 
                               const BigInt* modulus, BigInt* result);
int secure_validate_election_params(const ElectionParams* params);

// Integration with old collector module
int load_election_parameters(const char* config_file, ElectionParams* params);
int save_aggregation_result(const aggregation_result_t* result, const char* output_file);

// Backend API integration
enclave_result_t post_aggregation_to_backend(host_context_t* context, const aggregation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_PROCESSOR_H
