#include "enclave_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include "constants.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global enclave state
static collector_state_t g_collector_state = {0};
static bool g_enclave_initialized = false;

// Initialize the enclave state
enclave_result_t initialize_enclave_state(void) {
    if (g_enclave_initialized) {
        return ENCLAVE_SUCCESS;
    }

    memset(&g_collector_state, 0, sizeof(collector_state_t));
    g_collector_state.total_votes = 0;
    g_collector_state.valid_votes = 0;
    g_collector_state.invalid_votes = 0;
    g_collector_state.is_sealed = false;

    // Initialize aggregation data
    for (int i = 0; i < MAX_CANDIDATES; i++) {
        g_collector_state.aggregation.candidate_votes[i] = 0;
    }
    g_collector_state.aggregation.total_votes = 0;

    g_enclave_initialized = true;
    return ENCLAVE_SUCCESS;
}

// Cleanup enclave state
enclave_result_t cleanup_enclave_state(void) {
    if (!g_enclave_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    memset(&g_collector_state, 0, sizeof(collector_state_t));
    g_enclave_initialized = false;
    return ENCLAVE_SUCCESS;
}

// Get enclave information
enclave_result_t get_enclave_info(enclave_info_t* info) {
    if (!g_enclave_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!info) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    strncpy(info->version, "1.0.0", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    info->total_votes = g_collector_state.total_votes;
    info->valid_votes = g_collector_state.valid_votes;
    info->invalid_votes = g_collector_state.invalid_votes;
    info->is_sealed = g_collector_state.is_sealed;

    return ENCLAVE_SUCCESS;
}

// Process a vote
enclave_result_t process_vote(const vote_t* vote, vote_receipt_t* receipt) {
    if (!g_enclave_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!vote || !receipt) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Validate the vote first
    enclave_result_t result = validate_vote(vote);
    if (result != ENCLAVE_SUCCESS) {
        g_collector_state.invalid_votes++;
        return result;
    }

    // Update aggregation state
    result = update_aggregation_state(vote);
    if (result != ENCLAVE_SUCCESS) {
        g_collector_state.invalid_votes++;
        return result;
    }

    // Generate receipt
    memcpy(receipt->vote_id, vote->vote_id, VOTE_ID_SIZE);
    receipt->timestamp = vote->timestamp;
    receipt->status = VOTE_STATUS_ACCEPTED;
    
    // Compute receipt hash
    compute_vote_hash(vote, receipt->receipt_hash);

    g_collector_state.valid_votes++;
    g_collector_state.total_votes++;

    return ENCLAVE_SUCCESS;
}

// Validate a vote
enclave_result_t validate_vote(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check vote ID is not empty
    bool vote_id_empty = true;
    for (size_t i = 0; i < VOTE_ID_SIZE; i++) {
        if (vote->vote_id[i] != 0) {
            vote_id_empty = false;
            break;
        }
    }
    if (vote_id_empty) {
        return ENCLAVE_ERROR_INVALID_VOTE_ID;
    }

    // Check candidate ID is valid
    if (vote->candidate_id >= MAX_CANDIDATES) {
        return ENCLAVE_ERROR_INVALID_CANDIDATE;
    }

    // Check timestamp is reasonable (not too old or in future)
    // In a real implementation, you'd check against current time
    if (vote->timestamp == 0) {
        return ENCLAVE_ERROR_INVALID_TIMESTAMP;
    }

    // Verify vote signature
    enclave_result_t result = verify_vote_signature(vote);
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }

    return ENCLAVE_SUCCESS;
}

// Aggregate votes
enclave_result_t aggregate_votes(vote_aggregation_t* aggregation) {
    if (!g_enclave_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    if (!aggregation) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Copy current aggregation state
    memcpy(aggregation, &g_collector_state.aggregation, sizeof(vote_aggregation_t));

    return ENCLAVE_SUCCESS;
}

// Update aggregation state with a new vote
enclave_result_t update_aggregation_state(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (vote->candidate_id >= MAX_CANDIDATES) {
        return ENCLAVE_ERROR_INVALID_CANDIDATE;
    }

    // Increment vote count for the candidate
    g_collector_state.aggregation.candidate_votes[vote->candidate_id]++;
    g_collector_state.aggregation.total_votes++;

    return ENCLAVE_SUCCESS;
}

// Compute vote hash
enclave_result_t compute_vote_hash(const vote_t* vote, uint8_t* hash) {
    if (!vote || !hash) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Simple hash computation (in production, use SHA-256 or similar)
    // This is a placeholder implementation
    uint32_t simple_hash = 0;
    const uint8_t* data = (const uint8_t*)vote;
    
    for (size_t i = 0; i < sizeof(vote_t); i++) {
        simple_hash = ((simple_hash << 5) + simple_hash) + data[i];
    }

    // Store hash as bytes
    for (int i = 0; i < HASH_SIZE && i < 4; i++) {
        hash[i] = (uint8_t)((simple_hash >> (i * 8)) & 0xFF);
    }

    // Fill remaining bytes with zeros
    for (int i = 4; i < HASH_SIZE; i++) {
        hash[i] = 0;
    }

    return ENCLAVE_SUCCESS;
}

// Verify vote signature
enclave_result_t verify_vote_signature(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check if signature is not all zeros (basic validation)
    bool signature_empty = true;
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        if (vote->signature.data[i] != 0) {
            signature_empty = false;
            break;
        }
    }

    if (signature_empty) {
        return ENCLAVE_ERROR_INVALID_SIGNATURE;
    }

    // In a real implementation, you would:
    // 1. Extract public key from vote or lookup in registry
    // 2. Verify signature using cryptographic library
    // For simulation, we just check signature is not empty

    return ENCLAVE_SUCCESS;
}
