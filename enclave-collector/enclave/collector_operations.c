#include "enclave_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include "secure_crypto_processor.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global enclave state - simplified for auxiliary aggregation
static struct {
    crypto_params_t crypto_params;
    bool initialized;
    char aggregated_result[2048];
    size_t aux_count;
} g_enclave_state = {0};

// Initialize the enclave state
enclave_result_t initialize_enclave_state(void) {
    if (g_enclave_state.initialized) {
        return ENCLAVE_SUCCESS;
    }

    memset(&g_enclave_state, 0, sizeof(g_enclave_state));
    g_enclave_state.initialized = true;
    
    return ENCLAVE_SUCCESS;
}

// Cleanup enclave state
enclave_result_t cleanup_enclave_state(void) {
    if (!g_enclave_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    memset(&g_enclave_state, 0, sizeof(g_enclave_state));
    return ENCLAVE_SUCCESS;
}

// Set system parameters in enclave
enclave_result_t set_system_parameters(const crypto_params_t* params) {
    if (!params || !g_enclave_state.initialized) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    memcpy(&g_enclave_state.crypto_params, params, sizeof(crypto_params_t));
    return ENCLAVE_SUCCESS;
}

// Aggregate auxiliary values using secure computation
enclave_result_t aggregate_auxiliary_values(const api_auxiliary_value_t* aux_values, 
                                          size_t count, 
                                          char* result_buffer, 
                                          size_t buffer_size) {
    if (!g_enclave_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!aux_values || !result_buffer || count == 0 || buffer_size == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // TODO: Implement secure cryptographic aggregation
    // This should be done inside the enclave using the crypto_params
    // For now, create a mock aggregation result
    
    g_enclave_state.aux_count = count;
    
    // Mock aggregation: concatenate hashes or perform mathematical operations
    snprintf(result_buffer, buffer_size, "SECURE_AGG_%zu_VALUES", count);
    
    // Store result internally
    strncpy(g_enclave_state.aggregated_result, result_buffer, 
            sizeof(g_enclave_state.aggregated_result) - 1);
    
    return ENCLAVE_SUCCESS;
}

// Get aggregation result
enclave_result_t get_aggregation_result(char* result_buffer, size_t buffer_size) {
    if (!g_enclave_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!result_buffer || buffer_size == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (strlen(g_enclave_state.aggregated_result) == 0) {
        return ENCLAVE_ERROR_NO_DATA;
    }

    strncpy(result_buffer, g_enclave_state.aggregated_result, buffer_size - 1);
    result_buffer[buffer_size - 1] = '\0';
    
    return ENCLAVE_SUCCESS;
}

// Get auxiliary aggregation status
enclave_result_t get_auxiliary_status(size_t* aux_count, bool* has_result) {
    if (!g_enclave_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!aux_count || !has_result) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    *aux_count = g_enclave_state.aux_count;
    *has_result = (strlen(g_enclave_state.aggregated_result) > 0);
    
    return ENCLAVE_SUCCESS;
}
