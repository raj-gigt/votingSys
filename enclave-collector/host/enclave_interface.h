#ifndef ENCLAVE_INTERFACE_H
#define ENCLAVE_INTERFACE_H

#include "shared_types.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ENCLAVE CALLS (ECalls) - Essential glue functions between host and enclave
// ============================================================================

// Initialization and cleanup
int ecall_initialize_crypto_processor(
    const char* election_id,
    const char* N_hex,
    const char* H_hex,
    const char* skA_hex
);

int ecall_cleanup_auxiliary_collector(void);

// Core auxiliary value processing (main workflow)
int ecall_process_auxiliary_value(
    const char* aux_hex,
    const char* voter_id,
    uint64_t timestamp
);

int ecall_process_auxiliary_values_batch(
    const char** aux_hex_values,
    const char** voter_ids,
    const uint64_t* timestamps,
    size_t count,
    int* processed_count
);

int ecall_compute_final_aggregation(
    char* result_hex,
    size_t* result_hex_size,
    uint8_t* zk_proof,
    size_t* proof_size
);

// Simple state queries
int ecall_get_auxiliary_count(uint32_t* count);
int ecall_reset_auxiliary_collector(void);

// ============================================================================
// OUTSIDE CALLS (OCalls) - Functions called by enclave, executed by host
// ============================================================================

// Logging for auxiliary operations
void ocall_auxiliary_log_message(int level, const char* message);
void ocall_auxiliary_log_error(int error_code, const char* context);

// System operations
uint64_t ocall_get_timestamp(void);
int ocall_get_random_bytes(uint8_t* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // ENCLAVE_INTERFACE_H
