#include "secure_crypto_processor.h"
#include "enclave_operations.h"
#include "../../common/bigint_lib/bigint.h"
#include "error_codes.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// Forward declarations for internal functions
static enclave_result_t secure_verify_auxiliary_value_internal(const BigInt* aux_value, const char* voter_id);
static enclave_result_t secure_generate_aggregation_proof(const secure_crypto_state_t* state, uint8_t* zk_proof, size_t* proof_size);
static enclave_result_t secure_verify_zk_proof(const BigInt* aux_value, const BigInt* voter_pk, const secure_crypto_state_t* state);

// Secure memory operations
// Secure memory wiping (compiler-resistant)
static void secure_memzero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    volatile uint8_t* volatile_ptr = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = 0;
    }
}

// Secure memory comparison (constant-time to prevent timing attacks)
static enclave_result_t secure_memcmp(const void* a, const void* b, size_t size, int* result) {
    if (!a || !b || !result) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    const uint8_t* ptr_a = (const uint8_t*)a;
    const uint8_t* ptr_b = (const uint8_t*)b;
    
    int diff = 0;
    
    // Constant-time comparison to prevent timing attacks
    for (size_t i = 0; i < size; i++) {
        diff |= ptr_a[i] ^ ptr_b[i];
    }
    
    *result = diff;
    return ENCLAVE_SUCCESS;
}

// Secure global state within enclave - define only once
static secure_crypto_state_t g_crypto_state = {0};
static bool g_crypto_initialized = false;

// Math context structure for standalone mathematical operations
// This defines the internal structure of math_context_t that was forward-declared in the header
struct math_context_t {
    BigInt modulus;          // N parameter
    BigInt modulus_squared;  // N^2 parameter
    BigInt generator;        // H parameter
    BigInt running_product;  // Current product
    uint32_t operation_count;// Number of operations performed
    uint8_t initialized;
};

// Helper function to set BigInt to 1
static enclave_result_t set_bigint_to_one(BigInt* bigint) {
    if (!bigint) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Create a BigInt with value 1
    const char* one_hex = "1";
    return BigInt_from_hex_string(one_hex, bigint);
}

// Main crypto operations
enclave_result_t secure_crypto_init(const char* election_id, 
                                  const char* N_hex, 
                                  const char* H_hex, 
                                  const char* skA_hex) {
    if (!election_id || !N_hex || !H_hex || !skA_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (g_crypto_initialized) {
        return ENCLAVE_ERROR_ALREADY_INITIALIZED;
    }
    
    // Initialize crypto state
    memset(&g_crypto_state, 0, sizeof(g_crypto_state));
    strncpy(g_crypto_state.election_id, election_id, sizeof(g_crypto_state.election_id) - 1);
    
    // Initialize BigInt values
    enclave_result_t result;
    
    // Set N parameter
    result = BigInt_from_hex_string(N_hex, &g_crypto_state.N);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Set H parameter
    result = BigInt_from_hex_string(H_hex, &g_crypto_state.H);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Set secret key
    result = BigInt_from_hex_string(skA_hex, &g_crypto_state.skA);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Compute N^2 for efficiency
    result = BigInt_mul(&g_crypto_state.N, &g_crypto_state.N, &g_crypto_state.N_squared);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Compute public key: H^skA mod N^2
    result = BigInt_mod_exp(&g_crypto_state.H, &g_crypto_state.skA, 
                          &g_crypto_state.N_squared, &g_crypto_state.pk_A);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Initialize product to 1
    result = set_bigint_to_one(&g_crypto_state.aux_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    g_crypto_state.aux_count = 0;
    g_crypto_state.is_complete = false;
    
    g_crypto_initialized = true;
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_process_auxiliary_value(const char* aux_hex, 
                                              const char* voter_id, 
                                              uint64_t timestamp) {
    if (!aux_hex || !voter_id) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_crypto_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    if (g_crypto_state.is_complete) {
        return ENCLAVE_ERROR_INVALID_STATE;
    }
    
    // Check if we've reached the maximum number of auxiliary values
    if (g_crypto_state.aux_count >= MAX_AUX_VALUES) {
        return ENCLAVE_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Convert auxiliary value to BigInt
    BigInt aux_bigint;
    enclave_result_t result = BigInt_from_hex_string(aux_hex, &aux_bigint);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Verify the auxiliary value (anti-tampering check)
    result = secure_verify_auxiliary_value_internal(&aux_bigint, voter_id);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Update the product: product = (product * aux) mod N^2
    BigInt temp_product;
    result = BigInt_mul(&g_crypto_state.aux_product, &aux_bigint, &temp_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    result = BigInt_mod(&temp_product, &g_crypto_state.N_squared, &g_crypto_state.aux_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Record voter ID and timestamp for audit trail
    strncpy(g_crypto_state.processed_voters[g_crypto_state.aux_count].voter_id,
           voter_id, MAX_VOTER_ID_LENGTH - 1);
    g_crypto_state.processed_voters[g_crypto_state.aux_count].timestamp = timestamp;
    
    g_crypto_state.aux_count++;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_process_auxiliary_values_batch(const char** aux_hex_values,
                                                     const char** voter_ids,
                                                     const uint64_t* timestamps,
                                                     size_t count,
                                                     int* processed_count) {
    if (!aux_hex_values || !voter_ids || !timestamps || !processed_count) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_crypto_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    *processed_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        enclave_result_t result = secure_process_auxiliary_value(
            aux_hex_values[i], voter_ids[i], timestamps[i]);
        
        if (result != ENCLAVE_SUCCESS) {
            // Return with partial success
            return (i > 0) ? ENCLAVE_SUCCESS : result;
        }
        
        (*processed_count)++;
    }
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_compute_final_aggregation(char* result_hex, 
                                                size_t* result_hex_size,
                                                uint8_t* zk_proof, 
                                                size_t* proof_size) {
    if (!result_hex || !result_hex_size) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_crypto_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    if (g_crypto_state.aux_count == 0) {
        return ENCLAVE_ERROR_INVALID_STATE;
    }
    
    // Generate a ZK proof if requested
    if (zk_proof && proof_size) {
        enclave_result_t result = secure_generate_aggregation_proof(&g_crypto_state, zk_proof, proof_size);
        if (result != ENCLAVE_SUCCESS) return result;
    }
    
    // Convert the final product to a hex string
    enclave_result_t result = BigInt_to_hex_string(&g_crypto_state.aux_product, 
                                                 result_hex, *result_hex_size);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Update the size to reflect the actual length of the hex string
    *result_hex_size = strlen(result_hex);
    
    // Mark aggregation as complete
    g_crypto_state.is_complete = true;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_verify_auxiliary_value_crypto(const char* aux_hex,
                                                    const char* voter_public_key_hex,
                                                    int* is_valid) {
    if (!aux_hex || !voter_public_key_hex || !is_valid) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_crypto_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Convert auxiliary value and voter public key to BigInts
    BigInt aux_value, voter_pk;
    enclave_result_t result;
    
    result = BigInt_from_hex_string(aux_hex, &aux_value);
    if (result != ENCLAVE_SUCCESS) return result;
    
    result = BigInt_from_hex_string(voter_public_key_hex, &voter_pk);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Verify using zero-knowledge proof
    result = secure_verify_zk_proof(&aux_value, &voter_pk, &g_crypto_state);
    
    // Set validity based on verification result
    *is_valid = (result == ENCLAVE_SUCCESS);
    
    return ENCLAVE_SUCCESS;  // Return success even if verification fails to not leak info
}

enclave_result_t secure_crypto_cleanup(void) {
    if (!g_crypto_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Securely wipe sensitive data
    secure_memzero(&g_crypto_state.skA, sizeof(BigInt));
    secure_memzero(&g_crypto_state, sizeof(g_crypto_state));
    
    g_crypto_initialized = false;
    return ENCLAVE_SUCCESS;
}

// Math context operations (consolidated from secure_math)
enclave_result_t secure_math_init(math_context_t* context,
                                 const char* N_hex, 
                                 const char* H_hex, 
                                 const char* N_squared_hex) {
    if (!context || !N_hex || !H_hex || !N_squared_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    enclave_result_t result;
    
    // Initialize modulus
    result = BigInt_from_hex_string(N_hex, &context->modulus);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Initialize generator
    result = BigInt_from_hex_string(H_hex, &context->generator);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Initialize modulus squared
    result = BigInt_from_hex_string(N_squared_hex, &context->modulus_squared);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Initialize running product to 1
    result = set_bigint_to_one(&context->running_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    context->operation_count = 0;
    context->initialized = 1;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_math_cleanup(math_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Clear the context securely
    secure_memzero(context, sizeof(math_context_t));
    context->initialized = 0;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_math_process_auxiliary(math_context_t* context,
                                             const char* aux_value_hex) {
    if (!context || !aux_value_hex || !context->initialized) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Convert aux_value to BigInt
    BigInt aux_value;
    enclave_result_t result = BigInt_from_hex_string(aux_value_hex, &aux_value);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Multiply running product by aux_value: result = (product * aux_value) mod N^2
    BigInt temp_product;
    result = BigInt_mul(&context->running_product, &aux_value, &temp_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    result = BigInt_mod(&temp_product, &context->modulus_squared, &context->running_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Increment operation count
    context->operation_count++;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_math_get_product(const math_context_t* context,
                                       char* product_hex,
                                       size_t buffer_size) {
    if (!context || !product_hex || !context->initialized) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Convert running product to hex string
    enclave_result_t result = BigInt_to_hex_string(&context->running_product, 
                                                 product_hex, buffer_size);
    return result;
}

enclave_result_t secure_math_reset_product(math_context_t* context) {
    if (!context || !context->initialized) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Reset product to 1
    enclave_result_t result = set_bigint_to_one(&context->running_product);
    if (result != ENCLAVE_SUCCESS) return result;
    
    // Reset operation count
    context->operation_count = 0;
    
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_math_verify_auxiliary(const math_context_t* context,
                                            const char* aux_value_hex,
                                            const char* proof_hex) {
    if (!context || !aux_value_hex || !proof_hex || !context->initialized) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Implementation of the verification algorithm would go here
    // This is a placeholder that always returns success
    // In a real implementation, you would:
    // 1. Convert aux_value and proof to BigInt
    // 2. Verify the proof using the modulus and generator
    
    return ENCLAVE_SUCCESS;
}

// Internal helper functions

// Implementation of secure_verify_auxiliary_value_internal
static enclave_result_t secure_verify_auxiliary_value_internal(const BigInt* aux_value,
                                                             const char* voter_id) {
    // This function would implement a cryptographic verification
    // of the auxiliary value, potentially using the voter ID
    // This is a placeholder implementation that always returns success
    
    // In a real implementation, you would:
    // 1. Verify the auxiliary value is in the correct range
    // 2. Verify any embedded signatures or commitments
    
    return ENCLAVE_SUCCESS;
}

// Implementation of secure_generate_aggregation_proof
static enclave_result_t secure_generate_aggregation_proof(const secure_crypto_state_t* state,
                                                       uint8_t* zk_proof,
                                                       size_t* proof_size) {
    if (!state || !zk_proof || !proof_size) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // This is a placeholder implementation that generates a dummy proof
    // In a real implementation, you would generate a cryptographic proof
    
    // Fill proof with dummy data for now
    if (*proof_size < ZK_PROOF_SIZE) {
        *proof_size = ZK_PROOF_SIZE;
        return ENCLAVE_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Generate some deterministic but unpredictable data
    for (size_t i = 0; i < ZK_PROOF_SIZE; i++) {
        zk_proof[i] = (uint8_t)(i ^ (state->aux_count % 256));
    }
    
    *proof_size = ZK_PROOF_SIZE;
    return ENCLAVE_SUCCESS;
}

// Implementation of secure_verify_zk_proof
static enclave_result_t secure_verify_zk_proof(const BigInt* aux_value,
                                            const BigInt* voter_pk,
                                            const secure_crypto_state_t* state) {
    // This function would implement zero-knowledge proof verification
    // This is a placeholder implementation that always returns success
    
    // In a real implementation, you would:
    // 1. Verify the ZK proof against the auxiliary value and voter public key
    
    return ENCLAVE_SUCCESS;
}
