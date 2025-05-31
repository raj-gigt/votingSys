#include "host_interface.h"
#include "logging.h"
#include "api_client.h"
#include "shared_types.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#ifndef SIMULATION_MODE
#ifdef OE_BUILD_ENCLAVE
#include <openenclave/host.h>
#include "collector_u.h" // Generated from EDL
#endif
#endif

// Global simulation state for auxiliary aggregation
int g_sim_initialized = 0;
auxiliary_state_t g_sim_aux_state;

// Global host-side state for tracking auxiliary operations
static host_auxiliary_state_t g_host_aux_state = {0};

// Initialize host interface for auxiliary aggregation
enclave_result_t host_initialize(host_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }
    
    log_info("Initializing host interface for auxiliary aggregation...");
    
    // Clear context and set session ID
    host_config_t saved_config = context->config;
    memset(context, 0, sizeof(host_context_t));
    context->config = saved_config;
    context->session_id = (uint64_t)time(NULL);
    
    // Initialize auxiliary state
    memset(&g_host_aux_state, 0, sizeof(host_auxiliary_state_t));
    g_host_aux_state.initialized = true;
    g_host_aux_state.session_start = time(NULL);
    
#ifdef SIMULATION_MODE
    // Initialize simulation state
    memset(&g_sim_aux_state, 0, sizeof(auxiliary_state_t));
    g_sim_initialized = 1;
    log_info("Host interface initialized in simulation mode");
#else
    // Initialize hardware enclave if available
    log_info("Host interface initialized for hardware mode");
#endif
    
    log_info("Host interface initialized successfully for auxiliary aggregation");
    return ENCLAVE_SUCCESS;
}

// Cleanup host interface
enclave_result_t host_cleanup(host_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }
    
    log_info("Cleaning up host interface...");
    
    // Clear auxiliary state
    memset(&g_host_aux_state, 0, sizeof(host_auxiliary_state_t));
    
#ifdef SIMULATION_MODE
    g_sim_initialized = 0;
    memset(&g_sim_aux_state, 0, sizeof(auxiliary_state_t));
#endif
    
    // Clear context
    memset(context, 0, sizeof(host_context_t));
    
    log_info("Host interface cleanup completed");
    return ENCLAVE_SUCCESS;
}

// Process auxiliary aggregation request
enclave_result_t host_process_auxiliary_request(host_context_t* context, 
                                              const auxiliary_request_t* request,
                                              auxiliary_response_t* response) {
    if (!context || !request || !response) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }
    
    if (!g_host_aux_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    log_info("Processing auxiliary aggregation request");
    
    // Initialize response
    memset(response, 0, sizeof(auxiliary_response_t));
    response->request_id = request->request_id;
    response->timestamp = time(NULL);
    
    switch (request->type) {
        case AUX_REQ_AGGREGATE:
            log_info("Processing auxiliary aggregation request");
            response->result = host_handle_auxiliary_aggregation(context, 
                                                               request->aux_values,
                                                               request->count,
                                                               response->result_data);
            response->data_size = strlen(response->result_data);
            break;
            
        case AUX_REQ_STATUS:
            log_info("Processing auxiliary status request");
            response->result = host_get_auxiliary_status(context, response->result_data);
            response->data_size = strlen(response->result_data);
            break;
            
        default:
            log_error("Unknown auxiliary request type: %d", request->type);
            response->result = ENCLAVE_ERROR_INVALID_PARAMETER;
            break;
    }
    
    log_info("Auxiliary request processed with result: %d", response->result);
    return response->result;
}

// Handle auxiliary value aggregation
enclave_result_t host_handle_auxiliary_aggregation(host_context_t* context,
                                                 const auxiliary_value_t* aux_values,
                                                 size_t count,
                                                 char* result_buffer) {
    if (!context || !aux_values || !result_buffer || count == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    log_info("Handling aggregation of %zu auxiliary values", count);
    
#ifdef SIMULATION_MODE    // Simulation mode: simple aggregation
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        // Mock aggregation - in real implementation, this would be secure
        // Use first character of aux_hex as simple value for simulation
        sum += (aux_values[i].aux_hex[0] != '\0') ? (uint8_t)aux_values[i].aux_hex[0] : 0;
    }
    
    snprintf(result_buffer, 256, "SIM_AGG_RESULT_%llu_%zu", 
             (unsigned long long)sum, count);
    
    g_sim_aux_state.processed_count += count;
    log_info("Simulation aggregation completed: %s", result_buffer);
    
#else
    // Hardware mode: call secure enclave
    enclave_result_t result = ENCLAVE_ERROR_NOT_IMPLEMENTED;
    
#ifdef OE_BUILD_ENCLAVE
    // Call enclave function to perform secure aggregation
    oe_result_t oe_result = aggregate_auxiliary_values_enclave(
        g_enclave, &result, aux_values, count, result_buffer, 256);
    
    if (oe_result != OE_OK) {
        log_error("Enclave call failed: %s", oe_result_str(oe_result));
        return ENCLAVE_ERROR_ENCLAVE_CALL_FAILED;
    }
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Enclave aggregation failed: %d", result);
        return result;
    }
#else
    log_error("Hardware mode not supported - rebuild with SGX support");
    (void)result; // Suppress unused variable warning
    return ENCLAVE_ERROR_NOT_SUPPORTED;
#endif

#endif
    
    // Update state
    g_host_aux_state.processed_count += count;
    g_host_aux_state.last_aggregation = time(NULL);
    
    return ENCLAVE_SUCCESS;
}

// Get auxiliary aggregation status
enclave_result_t host_get_auxiliary_status(host_context_t* context, char* status_buffer) {
    if (!context || !status_buffer) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_host_aux_state.initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    // Create status report
    time_t uptime = time(NULL) - g_host_aux_state.session_start;
    
#ifdef SIMULATION_MODE
    snprintf(status_buffer, 512,
             "STATUS:SIM,PROCESSED:%zu,UPTIME:%ld,SESSION:0x%llx",
             g_host_aux_state.processed_count,
             uptime,
             (unsigned long long)context->session_id);
#else
    snprintf(status_buffer, 512,
             "STATUS:HW,PROCESSED:%zu,UPTIME:%ld,SESSION:0x%llx",
             g_host_aux_state.processed_count,
             uptime,
             (unsigned long long)context->session_id);
#endif
    
    log_info("Auxiliary status: %s", status_buffer);
    return ENCLAVE_SUCCESS;
}

// Get host configuration
enclave_result_t host_get_config(host_context_t* context, host_config_t* config) {
    if (!context || !config) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }
    
    memcpy(config, &context->config, sizeof(host_config_t));
    return ENCLAVE_SUCCESS;
}

// Set host configuration
enclave_result_t host_set_config(host_context_t* context, const host_config_t* config) {
    if (!context || !config) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }
    
    memcpy(&context->config, config, sizeof(host_config_t));
    log_info("Host configuration updated");
    return ENCLAVE_SUCCESS;
}
