#include "network_interface.h"
#include "host_interface.h"
#include "logging.h"
#include "shared_types.h"
#include "error_codes.h"
#include "api_client.h"
#include "secure_crypto_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Handle health check endpoint
enclave_result_t handle_health_check(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling health check request");

    const char* health_json = "{"
        "\"status\": \"healthy\","
        "\"timestamp\": %ld,"
        "\"service\": \"auxiliary-aggregator\","
        "\"version\": \"1.0.0\""
        "}";

    response->status_code = 200;
    response->body_length = strlen(health_json) + 32;
    response->body = malloc(response->body_length);
    
    if (!response->body) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    snprintf(response->body, response->body_length, health_json, time(NULL));
    response->body_length = strlen(response->body);

    return ENCLAVE_SUCCESS;
}

// Handle auxiliary value aggregation request
enclave_result_t handle_auxiliary_aggregation(const http_request_t* request, http_response_t* response) {
    if (!request || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_info("Starting auxiliary value aggregation process");

    // Step 1: Fetch system parameters from backend
    crypto_params_t crypto_params;
    enclave_result_t result = api_fetch_system_params(&crypto_params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch system parameters: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to fetch system parameters\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    log_debug("System parameters fetched successfully");

    // Step 2: Initialize enclave with crypto parameters
    result = secure_crypto_init(crypto_params.election_id, crypto_params.N, 
                               crypto_params.H, crypto_params.skA);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize enclave: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to initialize secure enclave\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    log_debug("Enclave initialized successfully");

    // Step 3: Fetch auxiliary values from backend
    api_auxiliary_value_t* aux_values = NULL;
    size_t aux_count = 0;
    result = api_fetch_auxiliary_values(&aux_values, &aux_count);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values: %s", get_error_description(result));
        secure_crypto_cleanup();
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to fetch auxiliary values\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    log_info("Fetched %zu auxiliary values from backend", aux_count);

    // Step 4: Process auxiliary values in enclave
    int processed_count = 0;
    for (size_t i = 0; i < aux_count; i++) {
        result = secure_process_auxiliary_value(
            aux_values[i].aux_value,
            aux_values[i].voter_id,
            aux_values[i].timestamp
        );
        
        if (result != ENCLAVE_SUCCESS) {
            log_warning("Failed to process auxiliary value %zu: %s", 
                       i, get_error_description(result));
            continue;
        }
        processed_count++;
    }

    log_info("Successfully processed %d out of %zu auxiliary values", 
             processed_count, aux_count);

    // Step 5: Compute final aggregation in enclave
    char result_hex[8192];
    size_t result_hex_size = sizeof(result_hex);
    uint8_t zk_proof[256];
    size_t proof_size = sizeof(zk_proof);

    result = secure_compute_final_aggregation(result_hex, &result_hex_size, 
                                            zk_proof, &proof_size);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to compute final aggregation: %s", get_error_description(result));
        api_free_auxiliary_values(aux_values, aux_count);
        secure_crypto_cleanup();
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to compute aggregation\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    log_info("Final aggregation computed successfully");

    // Step 6: Submit aggregated result back to backend
    result = api_submit_aux_product(result_hex);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to submit result to backend: %s", get_error_description(result));
        // Continue anyway - we have the result
    } else {
        log_info("Aggregation result submitted to backend successfully");
    }

    // Step 7: Create response
    char response_json[16384];
    snprintf(response_json, sizeof(response_json),
        "{"
        "\"status\": \"success\","
        "\"aggregation_result\": \"%s\","
        "\"processed_values\": %d,"
        "\"total_values\": %zu,"
        "\"zk_proof_included\": true,"
        "\"timestamp\": %ld,"
        "\"submitted_to_backend\": %s"
        "}",
        result_hex,
        processed_count,
        aux_count,
        time(NULL),
        (result == ENCLAVE_SUCCESS) ? "true" : "false");

    response->status_code = 200;
    response->body = strdup(response_json);
    response->body_length = strlen(response->body);

    // Cleanup
    api_free_auxiliary_values(aux_values, aux_count);
    secure_crypto_cleanup();

    log_info("Auxiliary aggregation completed successfully");
    return ENCLAVE_SUCCESS;
}

// Handle aggregation status request
enclave_result_t handle_aggregation_status(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling aggregation status request");

    // Get current system status
    char status_json[1024];
    snprintf(status_json, sizeof(status_json),
        "{"
        "\"service\": \"auxiliary-aggregator\","
        "\"status\": \"ready\","
        "\"timestamp\": %ld,"
        "\"last_aggregation\": null,"
        "\"enclave_ready\": true"
        "}",
        time(NULL));

    response->status_code = 200;
    response->body = strdup(status_json);
    response->body_length = strlen(response->body);

    return ENCLAVE_SUCCESS;
}

// Handle manual trigger for aggregation (for testing/admin)
enclave_result_t handle_trigger_aggregation(const http_request_t* request, http_response_t* response) {
    log_info("Manual aggregation trigger received");
    
    // Reuse the main aggregation handler
    return handle_auxiliary_aggregation(request, response);
}

// Handle system parameters request (GET /params)
enclave_result_t handle_system_params(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling system parameters request");

    // Fetch system parameters from API
    crypto_params_t crypto_params;
    enclave_result_t result = api_fetch_system_params(&crypto_params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch system parameters: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to fetch system parameters\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Create JSON response with system parameters
    char* params_json = malloc(8192);
    if (!params_json) {
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Memory allocation failed\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    snprintf(params_json, 8192,
        "{"
        "\"N\": \"%.100s...\","
        "\"H\": \"%.100s...\","
        "\"skA\": \"%.50s...\","
        "\"election_id\": \"%.50s\","
        "\"timestamp\": %ld"
        "}",
        crypto_params.N,
        crypto_params.H,
        crypto_params.skA,
        crypto_params.election_id,
        time(NULL));

    response->status_code = 200;
    response->body = params_json;
    response->body_length = strlen(params_json);

    log_debug("System parameters response sent");
    return ENCLAVE_SUCCESS;
}

// Handle fetch auxiliary values request (GET /fetch-auxiliary)
enclave_result_t handle_fetch_auxiliary(const http_request_t* request, http_response_t* response) {
    (void)request; // Unused parameter

    if (!response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling fetch auxiliary values request");

    // Fetch auxiliary values from API
    api_auxiliary_value_t* aux_values = NULL;
    size_t aux_count = 0;
    enclave_result_t result = api_fetch_auxiliary_values(&aux_values, &aux_count);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values: %s", get_error_description(result));
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to fetch auxiliary values\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Calculate required buffer size
    size_t buffer_size = 1024 + (aux_count * 512); // Base size + estimated per-value size
    char* aux_json = malloc(buffer_size);
    if (!aux_json) {
        api_free_auxiliary_values(aux_values, aux_count);
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Memory allocation failed\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Build JSON response
    strcpy(aux_json, "{\"auxiliaryValues\": [");
    
    for (size_t i = 0; i < aux_count; i++) {
        char value_json[512];
        snprintf(value_json, sizeof(value_json),
            "%s{\"voterId\": \"%.100s\", \"auxi\": \"%.200s\", \"timestamp\": %lu}",
            (i > 0) ? "," : "",
            aux_values[i].voter_id,
            aux_values[i].aux_value,
            aux_values[i].timestamp);
        
        strcat(aux_json, value_json);
    }
    
    strcat(aux_json, "]}");

    response->status_code = 200;
    response->body = aux_json;
    response->body_length = strlen(aux_json);

    // Cleanup
    api_free_auxiliary_values(aux_values, aux_count);

    log_debug("Auxiliary values response sent with %zu values", aux_count);
    return ENCLAVE_SUCCESS;
}

// Handle auxiliary submission request (POST /aux)
enclave_result_t handle_auxiliary_submission(const http_request_t* request, http_response_t* response) {
    if (!request || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Handling auxiliary submission request");

    // Check if this is a POST request
    if (strcmp(request->method, "POST") != 0) {
        response->status_code = 405;
        response->body = strdup("{\"error\": \"Method not allowed. Use POST.\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Extract aux product from request body
    if (!request->body || strlen(request->body) == 0) {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Missing request body with aux product\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Simple JSON parsing to extract "aux" field
    char* aux_start = strstr(request->body, "\"aux\"");
    if (!aux_start) {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Missing 'aux' field in request body\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Find the value after "aux":
    char* value_start = strchr(aux_start, ':');
    if (!value_start) {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Invalid JSON format\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    value_start++; // Skip ':'
    while (*value_start == ' ' || *value_start == '\t') value_start++; // Skip whitespace
    
    if (*value_start != '"') {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Aux value must be a string\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    value_start++; // Skip opening quote
    char* value_end = strchr(value_start, '"');
    if (!value_end) {
        response->status_code = 400;
        response->body = strdup("{\"error\": \"Unterminated aux value string\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Extract aux value
    size_t aux_length = value_end - value_start;
    char* aux_product = malloc(aux_length + 1);
    if (!aux_product) {
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Memory allocation failed\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    strncpy(aux_product, value_start, aux_length);
    aux_product[aux_length] = '\0';

    log_info("Received aux product submission: %.50s...", aux_product);

    // Submit to API backend
    enclave_result_t result = api_submit_aux_product(aux_product);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to submit aux product: %s", get_error_description(result));
        free(aux_product);
        response->status_code = 500;
        response->body = strdup("{\"error\": \"Failed to submit aux product to backend\"}");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }

    // Success response
    char success_json[512];
    snprintf(success_json, sizeof(success_json),
        "{"
        "\"status\": \"success\","
        "\"message\": \"Auxiliary product submitted successfully\","
        "\"aux_length\": %zu,"
        "\"timestamp\": %ld"
        "}",
        aux_length,
        time(NULL));

    response->status_code = 200;
    response->body = strdup(success_json);
    response->body_length = strlen(response->body);

    free(aux_product);

    log_info("Auxiliary submission completed successfully");
    return ENCLAVE_SUCCESS;
}
