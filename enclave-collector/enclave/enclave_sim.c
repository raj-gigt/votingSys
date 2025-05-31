#include "enclave_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include <stdio.h>
#include <string.h>

// Simulation mode enclave entry point
// This file implements ECALL functions for simulation mode

// ECALL: Initialize enclave
enclave_result_t ecall_initialize_enclave(void) {
    printf("[ENCLAVE SIM] Initializing enclave...\n");
    return initialize_enclave_state();
}

// ECALL: Get enclave information
enclave_result_t ecall_get_enclave_info(enclave_info_t* info) {
    printf("[ENCLAVE SIM] Getting enclave info...\n");
    return get_enclave_info(info);
}

// ECALL: Process auxiliary values
enclave_result_t ecall_process_auxiliary_values(const auxiliary_value_t* values, size_t count, 
                                               auxiliary_product_t* product) {
    printf("[ENCLAVE SIM] Processing %zu auxiliary values...\n", count);
    return process_auxiliary_values(values, count, product);
}

// ECALL: Validate auxiliary value
enclave_result_t ecall_validate_auxiliary_value(const auxiliary_value_t* value) {
    printf("[ENCLAVE SIM] Validating auxiliary value...\n");
    return validate_auxiliary_value(value);
}

// ECALL: Generate keypair
enclave_result_t ecall_generate_keypair(crypto_key_t* public_key, crypto_key_t* private_key) {
    printf("[ENCLAVE SIM] Generating keypair...\n");
    return generate_keypair(public_key, private_key);
}

// ECALL: Sign data
enclave_result_t ecall_sign_data(const uint8_t* data, size_t data_len, 
                                const crypto_key_t* private_key, crypto_signature_t* signature) {
    printf("[ENCLAVE SIM] Signing data (%zu bytes)...\n", data_len);
    return sign_data(data, data_len, private_key, signature);
}

// ECALL: Verify signature
enclave_result_t ecall_verify_signature(const uint8_t* data, size_t data_len,
                                       const crypto_signature_t* signature, 
                                       const crypto_key_t* public_key) {
    printf("[ENCLAVE SIM] Verifying signature (%zu bytes)...\n", data_len);
    return verify_signature(data, data_len, signature, public_key);
}

// ECALL: Aggregate auxiliary values
enclave_result_t ecall_aggregate_auxiliary_values(const api_auxiliary_value_t* values, size_t count,
                                                 char* result_buffer, size_t buffer_size) {
    printf("[ENCLAVE SIM] Aggregating %zu auxiliary values...\n", count);
    return aggregate_auxiliary_values(values, count, result_buffer, buffer_size);
}

// ECALL: Cleanup enclave
enclave_result_t ecall_cleanup_enclave(void) {
    printf("[ENCLAVE SIM] Cleaning up enclave...\n");
    return cleanup_enclave_state();
}
