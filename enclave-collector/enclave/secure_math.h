#ifndef SECURE_MATH_H
#define SECURE_MATH_H

#include "shared_types.h"
#include "error_codes.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Big integer structure for secure mathematical operations
typedef struct {
    uint8_t* data;
    size_t length;
    size_t capacity;
} secure_bigint_t;

// Mathematical operation context (no static data stored)
typedef struct {
    secure_bigint_t modulus;       // N parameter
    secure_bigint_t modulus_squared; // N^2 parameter
    secure_bigint_t generator;     // H parameter
    secure_bigint_t running_product; // Current product
    uint32_t operation_count;      // Number of operations performed
    uint8_t initialized;
} math_context_t;

// Initialize mathematical context with external parameters
enclave_result_t secure_math_init(math_context_t* context, 
                                 const char* N_hex, 
                                 const char* H_hex, 
                                 const char* N_squared_hex);

// Clean up mathematical context (secure memory wiping)
enclave_result_t secure_math_cleanup(math_context_t* context);

// Process auxiliary value (homomorphic multiplication)
enclave_result_t secure_math_process_auxiliary(math_context_t* context, 
                                              const char* aux_value_hex);

// Get current auxiliary product as hex string
enclave_result_t secure_math_get_product(const math_context_t* context, 
                                        char* product_hex, 
                                        size_t buffer_size);

// Reset the running product to 1
enclave_result_t secure_math_reset_product(math_context_t* context);

// Verify auxiliary value (optional cryptographic verification)
enclave_result_t secure_math_verify_auxiliary(const math_context_t* context,
                                             const char* aux_value_hex,
                                             const char* proof_hex);

// Big integer utility functions
enclave_result_t secure_bigint_from_hex(const char* hex_str, secure_bigint_t* bigint);
enclave_result_t secure_bigint_to_hex(const secure_bigint_t* bigint, char* hex_str, size_t buffer_size);
enclave_result_t secure_bigint_multiply_mod(const secure_bigint_t* a, 
                                           const secure_bigint_t* b, 
                                           const secure_bigint_t* mod, 
                                           secure_bigint_t* result);
void secure_bigint_free(secure_bigint_t* bigint);

// Secure memory operations
void secure_memzero(void* ptr, size_t size);
enclave_result_t secure_memcmp(const void* a, const void* b, size_t size, int* result);

#ifdef __cplusplus
}
#endif

#endif // SECURE_MATH_H
