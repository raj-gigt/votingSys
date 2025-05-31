#include "secure_crypto_interface.h"
#include "enclave_interface.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Static state for simulation mode
static struct {
    bool initialized;
    char election_id[128];
    uint32_t processed_count;
    char last_result[8192];
} g_sim_crypto_state = {0};

#ifdef SIMULATION_MODE

// Simulation mode implementations
enclave_result_t secure_crypto_init(const char* election_id, 
                                   const char* N_hex, 
                                   const char* H_hex, 
                                   const char* skA_hex) {
    if (!election_id || !N_hex || !H_hex || !skA_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_info("Initializing secure crypto in simulation mode");
    log_debug("Election ID: %s", election_id);

    // Initialize simulation state
    memset(&g_sim_crypto_state, 0, sizeof(g_sim_crypto_state));
    strncpy(g_sim_crypto_state.election_id, election_id, sizeof(g_sim_crypto_state.election_id) - 1);
    g_sim_crypto_state.initialized = true;
    g_sim_crypto_state.processed_count = 0;

    log_info("Secure crypto initialized successfully (simulation mode)");
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_process_auxiliary_value(const char* aux_hex, 
                                               const char* voter_id, 
                                               uint64_t timestamp) {
    if (!aux_hex || !voter_id) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!g_sim_crypto_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    log_debug("Processing auxiliary value from voter %s (simulation)", voter_id);

    // Simple simulation: just increment counter
    g_sim_crypto_state.processed_count++;

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

    if (!g_sim_crypto_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    log_info("Processing batch of %zu auxiliary values (simulation)", count);

    *processed_count = 0;
    for (size_t i = 0; i < count; i++) {
        enclave_result_t result = secure_process_auxiliary_value(
            aux_hex_values[i], voter_ids[i], timestamps[i]);
        
        if (result == ENCLAVE_SUCCESS) {
            (*processed_count)++;
        }
    }

    log_info("Processed %d out of %zu values in batch", *processed_count, count);
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_compute_final_aggregation(char* result_hex, 
                                                 size_t* result_hex_size,
                                                 uint8_t* zk_proof, 
                                                 size_t* proof_size) {
    if (!result_hex || !result_hex_size) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!g_sim_crypto_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }    log_info("Computing final aggregation (simulation mode)");

    // Generate a proper hex-encoded mock aggregation result
    // Create a mock 1024-bit (128 bytes) aggregated value to match expected length
    uint8_t mock_aggregated_bytes[128];
    
    // Fill with deterministic data based only on processed count (no time seed)
    uint32_t base_value = g_sim_crypto_state.processed_count;
    for (int i = 0; i < 128; i++) {
        // Use a deterministic pattern to generate varied bytes
        mock_aggregated_bytes[i] = (uint8_t)((base_value * (i + 1) * 0x1F + (i * 17) + (base_value >> (i % 8))) & 0xFF);
    }
    
    // Convert to hex string (128 bytes = 256 hex characters)
    size_t hex_len = 0;
    for (int i = 0; i < 128 && hex_len < *result_hex_size - 1; i++) {
        int written = snprintf(result_hex + hex_len, *result_hex_size - hex_len, 
                              "%02x", mock_aggregated_bytes[i]);
        if (written > 0) {
            hex_len += written;
        }
    }
    result_hex[hex_len] = '\0';
    *result_hex_size = hex_len;

    // Store result for future queries
    strncpy(g_sim_crypto_state.last_result, result_hex, sizeof(g_sim_crypto_state.last_result) - 1);

    // Generate mock ZK proof if requested
    if (zk_proof && proof_size) {
        uint8_t mock_proof[] = "MOCK_ZK_PROOF_SIMULATION";
        size_t mock_size = sizeof(mock_proof);
        
        if (*proof_size >= mock_size) {
            memcpy(zk_proof, mock_proof, mock_size);
            *proof_size = mock_size;
        } else {
            *proof_size = mock_size; // Return required size
            return ENCLAVE_ERROR_BUFFER_TOO_SMALL;
        }
    }

    log_info("Final aggregation computed: %s", result_hex);
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_verify_auxiliary_value_crypto(const char* aux_hex,
                                                     const char* voter_public_key_hex,
                                                     int* is_valid) {
    if (!aux_hex || !voter_public_key_hex || !is_valid) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!g_sim_crypto_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    log_debug("Verifying auxiliary value (simulation mode)");

    // In simulation mode, always return valid
    *is_valid = 1;

    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_crypto_cleanup(void) {
    if (!g_sim_crypto_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    log_info("Cleaning up secure crypto (simulation mode)");

    // Clear simulation state
    memset(&g_sim_crypto_state, 0, sizeof(g_sim_crypto_state));

    return ENCLAVE_SUCCESS;
}

#else // Hardware mode - use actual enclave calls

enclave_result_t secure_crypto_init(const char* election_id, 
                                   const char* N_hex, 
                                   const char* H_hex, 
                                   const char* skA_hex) {
    if (!election_id || !N_hex || !H_hex || !skA_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_info("Initializing secure crypto in hardware mode");

    // Call into enclave
    int result = ecall_initialize_crypto_processor(election_id, N_hex, H_hex, skA_hex);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize crypto processor in enclave: %d", result);
        return result;
    }

    log_info("Secure crypto initialized successfully (hardware mode)");
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_process_auxiliary_value(const char* aux_hex, 
                                               const char* voter_id, 
                                               uint64_t timestamp) {
    if (!aux_hex || !voter_id) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Processing auxiliary value from voter %s (hardware)", voter_id);

    // Call into enclave
    int result = ecall_process_auxiliary_value(aux_hex, voter_id, timestamp);
    
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to process auxiliary value in enclave: %d", result);
        return result;
    }

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

    log_info("Processing batch of %zu auxiliary values (hardware)", count);

    // Call into enclave
    int result = ecall_process_auxiliary_values_batch(aux_hex_values, voter_ids, 
                                                     timestamps, count, processed_count);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to process auxiliary values batch in enclave: %d", result);
        return result;
    }

    log_info("Processed %d out of %zu values in batch", *processed_count, count);
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_compute_final_aggregation(char* result_hex, 
                                                 size_t* result_hex_size,
                                                 uint8_t* zk_proof, 
                                                 size_t* proof_size) {
    if (!result_hex || !result_hex_size) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_info("Computing final aggregation (hardware mode)");

    // Call into enclave
    int result = ecall_compute_final_aggregation(result_hex, result_hex_size, 
                                               zk_proof, proof_size);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to compute final aggregation in enclave: %d", result);
        return result;
    }

    log_info("Final aggregation computed successfully");
    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_verify_auxiliary_value_crypto(const char* aux_hex,
                                                     const char* voter_public_key_hex,
                                                     int* is_valid) {
    if (!aux_hex || !voter_public_key_hex || !is_valid) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Verifying auxiliary value (hardware mode)");

    // Call into enclave
    int result = ecall_verify_auxiliary_value_cryptographically(aux_hex, 
                                                               voter_public_key_hex, 
                                                               is_valid);
    
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to verify auxiliary value in enclave: %d", result);
        return result;
    }

    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_crypto_cleanup(void) {
    log_info("Cleaning up secure crypto (hardware mode)");

    // Call into enclave cleanup if available
    // Note: Cleanup might be handled automatically by enclave destruction
    
    return ENCLAVE_SUCCESS;
}

#endif // SIMULATION_MODE

// Common implementations (both modes)

enclave_result_t secure_get_crypto_info(secure_crypto_info_t* info) {
    if (!info) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

#ifdef SIMULATION_MODE
    strncpy(info->election_id, g_sim_crypto_state.election_id, sizeof(info->election_id) - 1);
    info->aux_count = g_sim_crypto_state.processed_count;
    info->is_complete = (g_sim_crypto_state.processed_count > 0);
    info->is_initialized = g_sim_crypto_state.initialized;
#else
    // In hardware mode, would call into enclave to get this info
    memset(info, 0, sizeof(secure_crypto_info_t));
    info->is_initialized = true; // Assume initialized if we reach here
#endif

    return ENCLAVE_SUCCESS;
}

enclave_result_t secure_reset_crypto_state(void) {
    log_info("Resetting secure crypto state");

#ifdef SIMULATION_MODE
    g_sim_crypto_state.processed_count = 0;
    memset(g_sim_crypto_state.last_result, 0, sizeof(g_sim_crypto_state.last_result));
#else
    // In hardware mode, would call into enclave to reset state
#endif

    return ENCLAVE_SUCCESS;
}
