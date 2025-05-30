#include "election_management.h"
#include "api_client.h"
#include "logging.h"
#include "constants.h"
#include "shared_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global election context
static election_context_t g_election_context;
static bool g_election_management_initialized = false;

// Initialize election management system
enclave_result_t election_management_init(void) {
    if (g_election_management_initialized) {
        return ENCLAVE_SUCCESS;
    }

    log_info("Initializing election management system...");

    // Clear global context
    memset(&g_election_context, 0, sizeof(election_context_t));
    g_election_context.state = ELECTION_STATE_UNINITIALIZED;
    g_election_context.external_storage_enabled = true;

    g_election_management_initialized = true;
    log_info("Election management system initialized");
    return ENCLAVE_SUCCESS;
}

// Cleanup election management system
void election_management_cleanup(void) {
    if (!g_election_management_initialized) {
        return;
    }

    log_info("Cleaning up election management system...");

    // Clear sensitive data
    memset(&g_election_context, 0, sizeof(election_context_t));
    g_election_management_initialized = false;

    log_info("Election management system cleaned up");
}

// Initialize election from external API
enclave_result_t election_initialize_from_api(const char* election_id, 
                                             election_context_t* context) {
    if (!g_election_management_initialized || !election_id || !context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_info("Initializing election from API: %s", election_id);

    // Clear context
    memset(context, 0, sizeof(election_context_t));
    
    // Set election ID
    strncpy(context->election_id, election_id, MAX_ELECTION_ID_SIZE - 1);
    context->election_id[MAX_ELECTION_ID_SIZE - 1] = '\0';

    // Load election parameters from external API
    enclave_result_t result = election_load_from_api(election_id, 
                                                    &context->params, 
                                                    &context->aux_values);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to load election from API: %s", get_enclave_error_description(result));
        return result;
    }

    // Validate loaded parameters
    result = election_validate_parameters(&context->params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Invalid election parameters: %s", get_enclave_error_description(result));
        return result;
    }

    // Load election keys
    result = election_load_keys(election_id, &context->keys);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to load existing keys, will generate new ones");
        
        // Generate new keys if none exist
        // TODO: Implement key generation
        memset(&context->keys, 0, sizeof(key_pair_t));
        context->keys.public_key_size = PUBLIC_KEY_SIZE;
        context->keys.private_key_size = PRIVATE_KEY_SIZE;
        
        // Store generated keys
        result = election_store_keys(election_id, &context->keys);
        if (result != ENCLAVE_SUCCESS) {
            log_warning("Failed to store generated keys: %s", 
                       get_enclave_error_description(result));
        }
    }

    // Initialize context
    context->state = ELECTION_STATE_INITIALIZED;
    context->start_timestamp = 0;
    context->end_timestamp = 0;
    context->total_votes_received = 0;
    context->valid_votes_processed = 0;
    context->invalid_votes_rejected = 0;
    context->external_storage_enabled = true;

    // Update global context
    memcpy(&g_election_context, context, sizeof(election_context_t));

    log_info("Election initialized successfully: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Start election
enclave_result_t election_start(election_context_t* context) {
    if (!context || context->state != ELECTION_STATE_INITIALIZED) {
        return ENCLAVE_ERROR_INVALID_STATE;
    }

    log_info("Starting election: %s", context->election_id);

    context->state = ELECTION_STATE_ACTIVE;
    context->start_timestamp = (uint64_t)time(NULL);

    // Store progress to external API
    enclave_result_t result = election_store_progress(context);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to store election start progress: %s", 
                   get_enclave_error_description(result));
    }

    // Update global context
    memcpy(&g_election_context, context, sizeof(election_context_t));

    log_info("Election started: %s", context->election_id);
    return ENCLAVE_SUCCESS;
}

// Complete election
enclave_result_t election_complete(election_context_t* context) {
    if (!context || context->state != ELECTION_STATE_ACTIVE) {
        return ENCLAVE_ERROR_INVALID_STATE;
    }

    log_info("Completing election: %s", context->election_id);

    context->state = ELECTION_STATE_COMPLETED;
    context->end_timestamp = (uint64_t)time(NULL);

    // Store final progress to external API
    enclave_result_t result = election_store_progress(context);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to store election completion progress: %s", 
                   get_enclave_error_description(result));
    }

    // Update global context
    memcpy(&g_election_context, context, sizeof(election_context_t));

    log_info("Election completed: %s", context->election_id);
    return ENCLAVE_SUCCESS;
}

// Finalize election results
enclave_result_t election_finalize(election_context_t* context, 
                                  final_results_t* final_results) {
    if (!context || !final_results || context->state != ELECTION_STATE_COMPLETED) {
        return ENCLAVE_ERROR_INVALID_STATE;
    }

    log_info("Finalizing election results: %s", context->election_id);

    // Prepare final results
    strncpy(final_results->election_id, context->election_id, MAX_ELECTION_ID_SIZE - 1);
    final_results->election_id[MAX_ELECTION_ID_SIZE - 1] = '\0';
    final_results->total_votes = context->valid_votes_processed;
    final_results->timestamp = (uint64_t)time(NULL);
    final_results->candidate_count = context->params.num_candidates;

    // TODO: Calculate actual vote counts from enclave aggregation
    // For now, use context stats
    for (uint32_t i = 0; i < final_results->candidate_count && i < MAX_CANDIDATES; i++) {
        final_results->candidate_votes[i] = context->valid_votes_processed / final_results->candidate_count;
    }

    // Store final results to external API
    enclave_result_t result = api_store_final_results(final_results);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store final results: %s", get_enclave_error_description(result));
        return result;
    }

    context->state = ELECTION_STATE_FINALIZED;

    // Update global context
    memcpy(&g_election_context, context, sizeof(election_context_t));

    log_info("Election results finalized and stored externally: %s", context->election_id);
    return ENCLAVE_SUCCESS;
}

// Load election from API
enclave_result_t election_load_from_api(const char* election_id,
                                       election_params_t* params,
                                       auxiliary_values_t* aux_values) {
    if (!election_id || !params || !aux_values) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Loading election from API: %s", election_id);

    // Get election parameters from external API
    enclave_result_t result = api_get_election_parameters(params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to get election parameters: %s", get_enclave_error_description(result));
        return result;
    }

    // Get auxiliary values from external API
    result = api_get_auxiliary_values(aux_values);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to get auxiliary values: %s", get_enclave_error_description(result));
        return result;
    }

    log_debug("Election loaded from API successfully: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Validate election parameters
enclave_result_t election_validate_parameters(const election_params_t* params) {
    if (!params) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Validate basic parameters
    if (params->num_candidates == 0 || params->num_candidates > MAX_CANDIDATES) {
        log_error("Invalid number of candidates: %u", params->num_candidates);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (params->max_votes == 0) {
        log_error("Invalid max votes: %u", params->max_votes);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Validate key sizes
    if (params->public_key_size == 0 || params->public_key_size > PUBLIC_KEY_SIZE) {
        log_error("Invalid public key size: %zu", params->public_key_size);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Election parameters validated successfully");
    return ENCLAVE_SUCCESS;
}

// Store election progress
enclave_result_t election_store_progress(const election_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!context->external_storage_enabled) {
        log_debug("External storage disabled, skipping progress storage");
        return ENCLAVE_SUCCESS;
    }

    log_debug("Storing election progress: %s", context->election_id);

    // TODO: Implement progress storage API call
    // For now, just log the progress
    log_info("Election %s progress: state=%d, votes_received=%u, valid=%u, invalid=%u",
             context->election_id, context->state, 
             context->total_votes_received, 
             context->valid_votes_processed,
             context->invalid_votes_rejected);

    return ENCLAVE_SUCCESS;
}

// Load election keys
enclave_result_t election_load_keys(const char* election_id, key_pair_t* keys) {
    if (!election_id || !keys) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Loading election keys: %s", election_id);

    // Use API client to load keys
    enclave_result_t result = api_get_keys(keys);
    if (result != ENCLAVE_SUCCESS) {
        log_debug("Keys not found in external storage: %s", get_enclave_error_description(result));
        return result;
    }

    log_debug("Election keys loaded successfully: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Store election keys
enclave_result_t election_store_keys(const char* election_id, const key_pair_t* keys) {
    if (!election_id || !keys) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Storing election keys: %s", election_id);

    // Use API client to store keys
    enclave_result_t result = api_store_keys(keys);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store keys: %s", get_enclave_error_description(result));
        return result;
    }

    log_debug("Election keys stored successfully: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Get current election state
enclave_result_t election_get_state(const election_context_t* context, 
                                   election_state_t* state) {
    if (!context || !state) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    *state = context->state;
    return ENCLAVE_SUCCESS;
}
