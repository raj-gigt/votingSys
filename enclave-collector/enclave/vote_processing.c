#include "enclave_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include "constants.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Vote processing state
static vote_t processed_votes[MAX_VOTES_PER_BATCH];
static size_t processed_vote_count = 0;

// Process individual vote (called from process_vote)
static enclave_result_t process_individual_vote(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check if we have space for more votes
    if (processed_vote_count >= MAX_VOTES_PER_BATCH) {
        return ENCLAVE_ERROR_VOTE_BUFFER_FULL;
    }

    // Store processed vote
    memcpy(&processed_votes[processed_vote_count], vote, sizeof(vote_t));
    processed_vote_count++;

    return ENCLAVE_SUCCESS;
}

// Batch process votes for efficiency
enclave_result_t process_vote_batch(const vote_t* votes, size_t vote_count, 
                                   vote_receipt_t* receipts, size_t* processed_count) {
    if (!votes || !receipts || !processed_count || vote_count == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    *processed_count = 0;

    for (size_t i = 0; i < vote_count; i++) {
        enclave_result_t result = process_vote(&votes[i], &receipts[i]);
        if (result == ENCLAVE_SUCCESS) {
            (*processed_count)++;
        } else {
            // Log error but continue processing other votes
            // In a real implementation, you might want to stop on certain errors
            receipts[i].status = VOTE_STATUS_REJECTED;
            memcpy(receipts[i].vote_id, votes[i].vote_id, VOTE_ID_SIZE);
            receipts[i].timestamp = votes[i].timestamp;
        }
    }

    return ENCLAVE_SUCCESS;
}

// Validate vote format and constraints
enclave_result_t validate_vote_format(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check vote structure integrity
    if (vote->candidate_id >= MAX_CANDIDATES) {
        return ENCLAVE_ERROR_INVALID_CANDIDATE;
    }

    // Validate timestamp (should be within reasonable range)
    if (vote->timestamp == 0) {
        return ENCLAVE_ERROR_INVALID_TIMESTAMP;
    }

    // Check vote ID format (should not be all zeros)
    bool vote_id_valid = false;
    for (size_t i = 0; i < VOTE_ID_SIZE; i++) {
        if (vote->vote_id[i] != 0) {
            vote_id_valid = true;
            break;
        }
    }
    if (!vote_id_valid) {
        return ENCLAVE_ERROR_INVALID_VOTE_ID;
    }

    // Validate signature structure
    if (vote->signature.size != SIGNATURE_SIZE) {
        return ENCLAVE_ERROR_INVALID_SIGNATURE;
    }

    return ENCLAVE_SUCCESS;
}

// Check for duplicate votes
enclave_result_t check_duplicate_vote(const vote_t* vote) {
    if (!vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check against previously processed votes in current batch
    for (size_t i = 0; i < processed_vote_count; i++) {
        if (memcmp(processed_votes[i].vote_id, vote->vote_id, VOTE_ID_SIZE) == 0) {
            return ENCLAVE_ERROR_DUPLICATE_VOTE;
        }
    }

    return ENCLAVE_SUCCESS;
}

// Clear processed votes buffer
enclave_result_t clear_processed_votes(void) {
    processed_vote_count = 0;
    memset(processed_votes, 0, sizeof(processed_votes));
    return ENCLAVE_SUCCESS;
}

// Get vote processing statistics
enclave_result_t get_vote_processing_stats(vote_processing_stats_t* stats) {
    if (!stats) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    stats->processed_votes = processed_vote_count;
    stats->buffer_capacity = MAX_VOTES_PER_BATCH;
    stats->buffer_used = processed_vote_count;
    stats->buffer_available = MAX_VOTES_PER_BATCH - processed_vote_count;

    return ENCLAVE_SUCCESS;
}
