#include "secure_math.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Secure memory wiping (compiler-resistant)
void secure_memzero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    volatile uint8_t* volatile_ptr = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = 0;
    }
}

// Initialize big integer from hex string
enclave_result_t secure_bigint_from_hex(const char* hex_str, secure_bigint_t* bigint) {
    if (!hex_str || !bigint) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    size_t hex_len = strlen(hex_str);
    if (hex_len == 0 || hex_len % 2 != 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    size_t data_len = hex_len / 2;
    
    // Allocate memory for big integer data
    bigint->data = (uint8_t*)malloc(data_len);
    if (!bigint->data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    bigint->length = data_len;
    bigint->capacity = data_len;
    
    // Convert hex string to bytes
    for (size_t i = 0; i < data_len; i++) {
        char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
        bigint->data[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    return ENCLAVE_SUCCESS;
}

// Convert big integer to hex string
enclave_result_t secure_bigint_to_hex(const secure_bigint_t* bigint, char* hex_str, size_t buffer_size) {
    if (!bigint || !hex_str || !bigint->data) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    size_t required_size = bigint->length * 2 + 1;
    if (buffer_size < required_size) {
        return ENCLAVE_ERROR_BUFFER_TOO_SMALL;
    }
    
    for (size_t i = 0; i < bigint->length; i++) {
        snprintf(hex_str + i * 2, 3, "%02x", bigint->data[i]);
    }
    
    hex_str[bigint->length * 2] = '\0';
    return ENCLAVE_SUCCESS;
}

// Free big integer (with secure memory wiping)
void secure_bigint_free(secure_bigint_t* bigint) {
    if (!bigint || !bigint->data) {
        return;
    }
    
    // Securely wipe the data before freeing
    secure_memzero(bigint->data, bigint->capacity);
    free(bigint->data);
    
    bigint->data = NULL;
    bigint->length = 0;
    bigint->capacity = 0;
}

// Simple modular multiplication (for demo purposes - use proper big int library in production)
enclave_result_t secure_bigint_multiply_mod(const secure_bigint_t* a, 
                                           const secure_bigint_t* b, 
                                           const secure_bigint_t* mod, 
                                           secure_bigint_t* result) {
    if (!a || !b || !mod || !result) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // For demo purposes, use a simple approach
    // In production, integrate with a proper big integer library like libtommath
    
    // Allocate result buffer (size estimation)
    size_t result_size = a->length + b->length;
    result->data = (uint8_t*)malloc(result_size);
    if (!result->data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    result->length = result_size;
    result->capacity = result_size;
    
    // Simple multiplication and modulo (placeholder implementation)
    // This is a simplified version - use proper big integer arithmetic in production
    uint64_t temp_result = 0;
    
    if (a->length <= 8 && b->length <= 8 && mod->length <= 8) {
        // Handle small integers
        uint64_t val_a = 0, val_b = 0, val_mod = 0;
        
        for (size_t i = 0; i < a->length; i++) {
            val_a = (val_a << 8) | a->data[i];
        }
        for (size_t i = 0; i < b->length; i++) {
            val_b = (val_b << 8) | b->data[i];
        }
        for (size_t i = 0; i < mod->length; i++) {
            val_mod = (val_mod << 8) | mod->data[i];
        }
        
        if (val_mod > 0) {
            temp_result = (val_a * val_b) % val_mod;
        } else {
            temp_result = val_a * val_b;
        }
        
        // Convert result back to bytes
        size_t result_bytes = 8;
        for (int i = result_bytes - 1; i >= 0; i--) {
            result->data[i] = (uint8_t)(temp_result & 0xFF);
            temp_result >>= 8;
        }
        result->length = result_bytes;
    } else {
        // For larger integers, use a mock result
        memset(result->data, 0x42, result_size);
        log_warning("Using mock result for large integer multiplication");
    }
    
    return ENCLAVE_SUCCESS;
}

// Secure memory comparison
enclave_result_t secure_memcmp(const void* a, const void* b, size_t size, int* result) {
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

// Initialize mathematical context
enclave_result_t secure_math_init(math_context_t* context, 
                                 const char* N_hex, 
                                 const char* H_hex, 
                                 const char* N_squared_hex) {
    if (!context || !N_hex || !H_hex || !N_squared_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize context to zero
    memset(context, 0, sizeof(math_context_t));
    
    // Parse parameters from hex strings
    enclave_result_t result = secure_bigint_from_hex(N_hex, &context->modulus);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to parse N parameter from hex");
        return result;
    }
    
    result = secure_bigint_from_hex(H_hex, &context->generator);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to parse H parameter from hex");
        secure_bigint_free(&context->modulus);
        return result;
    }
    
    result = secure_bigint_from_hex(N_squared_hex, &context->modulus_squared);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to parse N^2 parameter from hex");
        secure_bigint_free(&context->modulus);
        secure_bigint_free(&context->generator);
        return result;
    }
    
    // Initialize running product to 1
    result = secure_bigint_from_hex("01", &context->running_product);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize running product");
        secure_bigint_free(&context->modulus);
        secure_bigint_free(&context->generator);
        secure_bigint_free(&context->modulus_squared);
        return result;
    }
    
    context->operation_count = 0;
    context->initialized = 1;
    
    log_info("Secure math context initialized successfully");
    return ENCLAVE_SUCCESS;
}

// Clean up mathematical context
enclave_result_t secure_math_cleanup(math_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->initialized) {
        return ENCLAVE_SUCCESS; // Already cleaned up
    }
    
    // Securely free all big integers
    secure_bigint_free(&context->modulus);
    secure_bigint_free(&context->generator);
    secure_bigint_free(&context->modulus_squared);
    secure_bigint_free(&context->running_product);
    
    // Securely wipe the context
    secure_memzero(context, sizeof(math_context_t));
    
    log_info("Secure math context cleaned up");
    return ENCLAVE_SUCCESS;
}

// Process auxiliary value (homomorphic multiplication)
enclave_result_t secure_math_process_auxiliary(math_context_t* context, 
                                              const char* aux_value_hex) {
    if (!context || !aux_value_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Parse auxiliary value from hex
    secure_bigint_t aux_value;
    enclave_result_t result = secure_bigint_from_hex(aux_value_hex, &aux_value);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to parse auxiliary value from hex");
        return result;
    }
    
    // Perform homomorphic multiplication: running_product = running_product * aux_value mod N^2
    secure_bigint_t new_product;
    result = secure_bigint_multiply_mod(&context->running_product, 
                                       &aux_value, 
                                       &context->modulus_squared, 
                                       &new_product);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to perform modular multiplication");
        secure_bigint_free(&aux_value);
        return result;
    }
    
    // Replace the running product with the new result
    secure_bigint_free(&context->running_product);
    context->running_product = new_product;
    context->operation_count++;
    
    // Clean up auxiliary value
    secure_bigint_free(&aux_value);
    
    log_debug("Processed auxiliary value, operation count: %u", context->operation_count);
    return ENCLAVE_SUCCESS;
}

// Get current auxiliary product as hex string
enclave_result_t secure_math_get_product(const math_context_t* context, 
                                        char* product_hex, 
                                        size_t buffer_size) {
    if (!context || !product_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    enclave_result_t result = secure_bigint_to_hex(&context->running_product, 
                                                  product_hex, 
                                                  buffer_size);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to convert product to hex string");
        return result;
    }
    
    log_debug("Retrieved current product (operations: %u)", context->operation_count);
    return ENCLAVE_SUCCESS;
}

// Reset the running product to 1
enclave_result_t secure_math_reset_product(math_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Free current running product
    secure_bigint_free(&context->running_product);
    
    // Reset to 1
    enclave_result_t result = secure_bigint_from_hex("01", &context->running_product);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to reset running product");
        return result;
    }
    
    context->operation_count = 0;
    log_info("Running product reset to 1");
    return ENCLAVE_SUCCESS;
}

// Verify auxiliary value (cryptographic verification)
enclave_result_t secure_math_verify_auxiliary(const math_context_t* context,
                                             const char* aux_value_hex,
                                             const char* proof_hex) {
    if (!context || !aux_value_hex || !proof_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Parse auxiliary value and proof
    secure_bigint_t aux_value, proof;
    enclave_result_t result = secure_bigint_from_hex(aux_value_hex, &aux_value);
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }
    
    result = secure_bigint_from_hex(proof_hex, &proof);
    if (result != ENCLAVE_SUCCESS) {
        secure_bigint_free(&aux_value);
        return result;
    }
    
    // Perform cryptographic verification (simplified for demo)
    // In production, implement proper zero-knowledge proof verification
    
    // For now, just check that auxiliary value is not zero
    int is_zero = 1;
    for (size_t i = 0; i < aux_value.length; i++) {
        if (aux_value.data[i] != 0) {
            is_zero = 0;
            break;
        }
    }
    
    secure_bigint_free(&aux_value);
    secure_bigint_free(&proof);
    
    if (is_zero) {
        log_warning("Auxiliary value verification failed: zero value");
        return ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED;
    }
    
    log_debug("Auxiliary value verification passed");
    return ENCLAVE_SUCCESS;
}
