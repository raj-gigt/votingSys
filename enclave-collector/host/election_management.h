#ifndef ELECTION_MANAGEMENT_H
#define ELECTION_MANAGEMENT_H

#include "shared_types.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Election Management Module
 * 
 * This module handles election lifecycle and integrates with external API
 * for dynamic election parameter loading and result storage.
 */

// Election state
typedef enum {
    ELECTION_STATE_UNINITIALIZED = 0,
    ELECTION_STATE_INITIALIZED,
    ELECTION_STATE_ACTIVE,
    ELECTION_STATE_PAUSED,
    ELECTION_STATE_COMPLETED,
    ELECTION_STATE_FINALIZED
} election_state_t;

// Election context
typedef struct {
    election_state_t state;
    election_params_t params;
    auxiliary_values_t aux_values;
    key_pair_t keys;
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    uint32_t total_votes_received;
    uint32_t valid_votes_processed;
    uint32_t invalid_votes_rejected;
    char election_id[MAX_ELECTION_ID_SIZE];
    bool external_storage_enabled;
} election_context_t;

// Function declarations

/**
 * Initialize election management system
 */
enclave_result_t election_management_init(void);

/**
 * Cleanup election management system
 */
void election_management_cleanup(void);

/**
 * Initialize a new election from external API parameters
 * @param election_id Unique identifier for the election
 * @param context Output election context
 */
enclave_result_t election_initialize_from_api(const char* election_id, 
                                             election_context_t* context);

/**
 * Start an initialized election
 * @param context Election context
 */
enclave_result_t election_start(election_context_t* context);

/**
 * Pause an active election
 * @param context Election context
 */
enclave_result_t election_pause(election_context_t* context);

/**
 * Resume a paused election
 * @param context Election context
 */
enclave_result_t election_resume(election_context_t* context);

/**
 * Complete an election (stop accepting votes)
 * @param context Election context
 */
enclave_result_t election_complete(election_context_t* context);

/**
 * Finalize election results and store to external API
 * @param context Election context
 * @param final_results Output final results
 */
enclave_result_t election_finalize(election_context_t* context, 
                                  final_results_t* final_results);

/**
 * Get current election state
 * @param context Election context
 * @param state Output election state
 */
enclave_result_t election_get_state(const election_context_t* context, 
                                   election_state_t* state);

/**
 * Validate election parameters from external source
 * @param params Election parameters to validate
 */
enclave_result_t election_validate_parameters(const election_params_t* params);

/**
 * Load election configuration from external API
 * @param election_id Election identifier
 * @param params Output election parameters
 * @param aux_values Output auxiliary values
 */
enclave_result_t election_load_from_api(const char* election_id,
                                       election_params_t* params,
                                       auxiliary_values_t* aux_values);

/**
 * Store election progress to external API
 * @param context Election context
 */
enclave_result_t election_store_progress(const election_context_t* context);

/**
 * Load election keys from external storage
 * @param election_id Election identifier
 * @param keys Output key pair
 */
enclave_result_t election_load_keys(const char* election_id, key_pair_t* keys);

/**
 * Store election keys to external storage
 * @param election_id Election identifier
 * @param keys Key pair to store
 */
enclave_result_t election_store_keys(const char* election_id, const key_pair_t* keys);

#ifdef __cplusplus
}
#endif

#endif // ELECTION_MANAGEMENT_H
