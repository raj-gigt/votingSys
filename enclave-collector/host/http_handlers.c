#include "network_interface.h"
#include "host_interface.h"
#include "logging.h"
#include "shared_types.h"
#include "error_codes.h"
#include "api_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// JSON parsing helpers (simple implementation for demo)
static enclave_result_t parse_vote_json(const char* json, vote_t* vote);

// Handle health check endpoint
enclave_result_t handle_health_check(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling health check request");

    // Simple health check response
    const char* health_json = "{"
        "\"status\": \"healthy\","
        "\"timestamp\": %ld,"
        "\"service\": \"enclave-collector\","
        "\"version\": \"1.0.0\""
        "}";

    response->status_code = 200;
    response->body_length = strlen(health_json) + 32; // Extra space for timestamp
    response->body = malloc(response->body_length);
    
    if (!response->body) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    snprintf(response->body, response->body_length, health_json, time(NULL));
    response->body_length = strlen(response->body);

    return ENCLAVE_SUCCESS;
}

// Handle vote submission
enclave_result_t handle_vote_submission(const http_request_t* request, http_response_t* response) {
    if (!request || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling vote submission request");

    if (!request->body || request->body_length == 0) {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Missing vote data\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Parse vote from JSON
    vote_t vote;
    enclave_result_t result = parse_vote_json(request->body, &vote);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to parse vote JSON: %s", get_error_description(result));
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Invalid vote format\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Get election parameters from external API before processing
    election_params_t election_params;
    result = api_get_election_parameters(&election_params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch election parameters: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Election parameters unavailable\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Get auxiliary values from external API
    auxiliary_values_t aux_values;
    result = api_get_auxiliary_values(&aux_values);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Auxiliary values unavailable\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Process vote through enclave with external data
    vote_receipt_t receipt;
    result = host_process_vote(&vote, &receipt);
    
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to process vote: %s", get_error_description(result));
        response->status_code = 400;
        
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
            "{\"error\": \"Vote processing failed\", \"code\": %d}", result);
        response->body = strdup(error_msg);
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Store vote result to external API
    result = api_store_vote_receipt(&receipt);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to store vote receipt: %s", get_error_description(result));
        // Continue anyway as vote was processed successfully
    }

    // Create success response with receipt
    char receipt_json[1024];
    snprintf(receipt_json, sizeof(receipt_json),
        "{"
        "\"status\": \"success\","
        "\"receipt\": {"
            "\"vote_id\": \"");
    
    // Add vote ID as hex string
    size_t pos = strlen(receipt_json);
    for (int i = 0; i < VOTE_ID_SIZE && pos < sizeof(receipt_json) - 32; i++) {
        pos += snprintf(receipt_json + pos, sizeof(receipt_json) - pos, 
            "%02x", receipt.vote_id[i]);
    }
    
    snprintf(receipt_json + pos, sizeof(receipt_json) - pos,
        "\","
        "\"timestamp\": %ld,"
        "\"status\": %d"
        "}"
        "}", receipt.timestamp, receipt.status);

    response->status_code = 200;
    response->body = strdup(receipt_json);
    response->body_length = strlen(response->body);

    log_info("Vote processed successfully and stored externally");
    return ENCLAVE_SUCCESS;
}

// Handle vote aggregation request
enclave_result_t handle_vote_aggregation(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling vote aggregation request");

    // Get aggregation from enclave (computed securely)
    vote_aggregation_t aggregation;
    enclave_result_t result = host_aggregate_votes(&aggregation);
    
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to get vote aggregation: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to get aggregation\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Store final results to external API
    final_results_t final_results;
    final_results.total_votes = aggregation.total_votes;
    final_results.candidate_count = MAX_CANDIDATES;
    memcpy(final_results.candidate_votes, aggregation.candidate_votes, 
           sizeof(uint32_t) * MAX_CANDIDATES);
    final_results.timestamp = (uint64_t)time(NULL);

    result = api_store_final_results(&final_results);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to store final results: %s", get_error_description(result));
        // Continue anyway as aggregation was computed successfully
    }

    // Create aggregation JSON response
    char aggregation_json[2048];
    snprintf(aggregation_json, sizeof(aggregation_json),
        "{"
        "\"total_votes\": %u,"
        "\"timestamp\": %lu,"
        "\"stored_externally\": true,"
        "\"candidates\": [",
        aggregation.total_votes, final_results.timestamp);

    size_t pos = strlen(aggregation_json);
    for (int i = 0; i < MAX_CANDIDATES && pos < sizeof(aggregation_json) - 64; i++) {
        if (i > 0) {
            pos += snprintf(aggregation_json + pos, sizeof(aggregation_json) - pos, ",");
        }
        pos += snprintf(aggregation_json + pos, sizeof(aggregation_json) - pos,
            "{\"candidate_id\": %d, \"votes\": %u}", 
            i, aggregation.candidate_votes[i]);
    }

    snprintf(aggregation_json + pos, sizeof(aggregation_json) - pos, "]}");

    response->status_code = 200;
    response->body = strdup(aggregation_json);
    response->body_length = strlen(response->body);

    log_info("Vote aggregation completed and stored externally");
    return ENCLAVE_SUCCESS;
}

// Handle enclave info request
enclave_result_t handle_enclave_info(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling enclave info request");

    // Get enclave information
    enclave_info_t info;
    enclave_result_t result = host_get_enclave_info(&info);
    
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to get enclave info: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to get enclave info\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Create info JSON response
    char info_json[1024];
    snprintf(info_json, sizeof(info_json),
        "{"
        "\"version\": \"%s\","
        "\"total_votes\": %u,"
        "\"valid_votes\": %u,"
        "\"invalid_votes\": %u,"
        "\"is_sealed\": %s"
        "}",
        info.version,
        info.total_votes,
        info.valid_votes,
        info.invalid_votes,
        info.is_sealed ? "true" : "false");

    response->status_code = 200;
    response->body = strdup(info_json);
    response->body_length = strlen(response->body);

    return ENCLAVE_SUCCESS;
}

// Simple JSON vote parser (basic implementation)
static enclave_result_t parse_vote_json(const char* json, vote_t* vote) {
    if (!json || !vote) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    memset(vote, 0, sizeof(vote_t));

    // Simple parsing - in production use a proper JSON library
    char candidate_str[32];
    char timestamp_str[32];
    char vote_id_str[128];

    // Extract candidate_id
    const char* candidate_pos = strstr(json, "\"candidate_id\":");
    if (candidate_pos) {
        sscanf(candidate_pos + 15, "%31s", candidate_str);
        vote->candidate_id = (uint32_t)atoi(candidate_str);
    }

    // Extract timestamp
    const char* timestamp_pos = strstr(json, "\"timestamp\":");
    if (timestamp_pos) {
        sscanf(timestamp_pos + 12, "%31s", timestamp_str);
        vote->timestamp = (uint64_t)atoll(timestamp_str);
    }

    // Extract vote_id (as hex string)
    const char* vote_id_pos = strstr(json, "\"vote_id\":");
    if (vote_id_pos) {
        const char* id_start = strchr(vote_id_pos + 10, '"');
        if (id_start) {
            id_start++; // Skip opening quote
            const char* id_end = strchr(id_start, '"');
            if (id_end) {
                size_t id_len = id_end - id_start;
                if (id_len < sizeof(vote_id_str)) {
                    strncpy(vote_id_str, id_start, id_len);
                    vote_id_str[id_len] = '\0';
                    
                    // Convert hex string to bytes
                    for (size_t i = 0; i < VOTE_ID_SIZE && i * 2 < id_len; i++) {
                        sscanf(vote_id_str + i * 2, "%2hhx", &vote->vote_id[i]);
                    }
                }
            }
        }
    }

    // Set default timestamp if not provided
    if (vote->timestamp == 0) {
        vote->timestamp = (uint64_t)time(NULL);
    }

    // Create dummy signature for simulation
    vote->signature.size = SIGNATURE_SIZE;
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        vote->signature.data[i] = (uint8_t)(0x42 + (i % 16)); // Dummy signature
    }

    return ENCLAVE_SUCCESS;
}
