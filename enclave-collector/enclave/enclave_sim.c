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

// ECALL: Process vote
enclave_result_t ecall_process_vote(const vote_t* vote, vote_receipt_t* receipt) {
    printf("[ENCLAVE SIM] Processing vote ID: ");
    for (int i = 0; i < 8 && i < VOTE_ID_SIZE; i++) {
        printf("%02x", vote->vote_id[i]);
    }
    printf("...\n");
    
    return process_vote(vote, receipt);
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

// ECALL: Aggregate votes
enclave_result_t ecall_aggregate_votes(vote_aggregation_t* aggregation) {
    printf("[ENCLAVE SIM] Aggregating votes...\n");
    return aggregate_votes(aggregation);
}

// ECALL: Seal data
enclave_result_t ecall_seal_data(const uint8_t* data, size_t data_len, 
                                uint8_t** sealed_data, size_t* sealed_len) {
    printf("[ENCLAVE SIM] Sealing data (%zu bytes)...\n", data_len);
    return seal_data(data, data_len, sealed_data, sealed_len);
}

// ECALL: Unseal data
enclave_result_t ecall_unseal_data(const uint8_t* sealed_data, size_t sealed_len,
                                  uint8_t** data, size_t* data_len) {
    printf("[ENCLAVE SIM] Unsealing data (%zu bytes)...\n", sealed_len);
    return unseal_data(sealed_data, sealed_len, data, data_len);
}

// ECALL: Cleanup enclave
enclave_result_t ecall_cleanup_enclave(void) {
    printf("[ENCLAVE SIM] Cleaning up enclave...\n");
    return cleanup_enclave_state();
}
