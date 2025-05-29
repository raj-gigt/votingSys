#include "api_client.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <curl/curl.h>
#endif

// Global API client state
static api_config_t g_api_config = {0};
static int g_api_initialized = 0;

// HTTP response data structure
typedef struct {
    char* data;
    size_t size;
} http_response_data_t;

// Callback for writing HTTP response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, http_response_data_t* response) {
    size_t total_size = size * nmemb;
    
    char* new_data = realloc(response->data, response->size + total_size + 1);
    if (!new_data) {
        log_error("Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = new_data;
    memcpy(response->data + response->size, contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

// Initialize API client
enclave_result_t api_client_init(const api_config_t* config) {
    if (!config) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (g_api_initialized) {
        log_warning("API client already initialized");
        return ENCLAVE_SUCCESS;
    }
    
    // Copy configuration
    memcpy(&g_api_config, config, sizeof(api_config_t));
    
#ifdef _WIN32
    // Initialize Winsock for Windows
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_error("WSAStartup failed: %d", result);
        return ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED;
    }
#else
    // Initialize libcurl
    CURLcode curl_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_result != CURLE_OK) {
        log_error("curl_global_init failed: %s", curl_easy_strerror(curl_result));
        return ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED;
    }
#endif
    
    g_api_initialized = 1;
    log_info("API client initialized with base URL: %s", g_api_config.base_url);
    return ENCLAVE_SUCCESS;
}

// Cleanup API client
void api_client_cleanup(void) {
    if (!g_api_initialized) {
        return;
    }
    
#ifdef _WIN32
    WSACleanup();
#else
    curl_global_cleanup();
#endif
    
    memset(&g_api_config, 0, sizeof(api_config_t));
    g_api_initialized = 0;
    log_info("API client cleaned up");
}

// Perform HTTP GET request
static enclave_result_t http_get(const char* endpoint, http_response_data_t* response) {
    if (!endpoint || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    char url[1024];
    snprintf(url, sizeof(url), "%s%s", g_api_config.base_url, endpoint);
    
    log_debug("HTTP GET: %s", url);
    
#ifdef _WIN32
    // Simple HTTP implementation for Windows (basic implementation)
    response->data = strdup("{\"status\": \"mock_response\"}");
    response->size = strlen(response->data);
    return ENCLAVE_SUCCESS;
#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize response
    response->data = NULL;
    response->size = 0;
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_api_config.timeout_ms / 1000);
    
    // Add auth header if available
    struct curl_slist* headers = NULL;
    if (strlen(g_api_config.auth_token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_api_config.auth_token);
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("HTTP GET failed: %s", curl_easy_strerror(res));
        if (response->data) {
            free(response->data);
            response->data = NULL;
            response->size = 0;
        }
        return ENCLAVE_ERROR_NETWORK_REQUEST_FAILED;
    }
    
    return ENCLAVE_SUCCESS;
#endif
}

// Perform HTTP POST request
static enclave_result_t http_post(const char* endpoint, const char* json_data, http_response_data_t* response) {
    if (!endpoint || !json_data || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    char url[1024];
    snprintf(url, sizeof(url), "%s%s", g_api_config.base_url, endpoint);
    
    log_debug("HTTP POST: %s", url);
    
#ifdef _WIN32
    // Simple HTTP implementation for Windows (basic implementation)
    response->data = strdup("{\"success\": true}");
    response->size = strlen(response->data);
    return ENCLAVE_SUCCESS;
#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize response
    response->data = NULL;
    response->size = 0;
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_api_config.timeout_ms / 1000);
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (strlen(g_api_config.auth_token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_api_config.auth_token);
        headers = curl_slist_append(headers, auth_header);
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("HTTP POST failed: %s", curl_easy_strerror(res));
        if (response->data) {
            free(response->data);
            response->data = NULL;
            response->size = 0;
        }
        return ENCLAVE_ERROR_NETWORK_REQUEST_FAILED;
    }
    
    return ENCLAVE_SUCCESS;
#endif
}

// Fetch election parameters from external API
enclave_result_t api_fetch_election_params(const char* election_id, crypto_params_t* params) {
    if (!election_id || !params) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/collector/params?election_id=%s", election_id);
    
    http_response_data_t response;
    enclave_result_t result = http_get(endpoint, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }
    
    // Simple JSON parsing (in a real implementation, use a proper JSON library)
    // For now, mock the response
    strcpy(params->N, "mock_N_parameter");
    strcpy(params->H, "mock_H_parameter");
    strcpy(params->N_squared, "mock_N_squared_parameter");
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Fetched election parameters for election: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Submit auxiliary product to external API
enclave_result_t api_submit_auxiliary_product(const char* election_id, const char* product_hex) {
    if (!election_id || !product_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/collector/aux");
    
    char json_data[2048];
    snprintf(json_data, sizeof(json_data), 
        "{\"election_id\": \"%s\", \"aux\": \"%s\"}", 
        election_id, product_hex);
    
    http_response_data_t response;
    enclave_result_t result = http_post(endpoint, json_data, &response);
    
    if (response.data) {
        free(response.data);
    }
    
    if (result == ENCLAVE_SUCCESS) {
        log_info("Successfully submitted auxiliary product for election: %s", election_id);
    }
    
    return result;
}

// Fetch auxiliary values from external API
enclave_result_t api_fetch_auxiliary_values(const char* election_id, auxiliary_value_t** values, size_t* count) {
    if (!election_id || !values || !count) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/collector/fetch-auxiliary?election_id=%s", election_id);
    
    http_response_data_t response;
    enclave_result_t result = http_get(endpoint, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }
    
    // Mock response for now (in real implementation, parse JSON)
    *count = 2;
    *values = malloc(sizeof(auxiliary_value_t) * (*count));
    if (!*values) {
        if (response.data) free(response.data);
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    strcpy((*values)[0].voter_id, "voter_001");
    strcpy((*values)[0].aux_value, "mock_aux_value_1");
    strcpy((*values)[1].voter_id, "voter_002");
    strcpy((*values)[1].aux_value, "mock_aux_value_2");
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Fetched %zu auxiliary values for election: %s", *count, election_id);
    return ENCLAVE_SUCCESS;
}

// Submit vote result
enclave_result_t api_submit_vote_result(const char* election_id, const vote_receipt_t* receipt) {
    if (!election_id || !receipt) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/collector/vote-result");
    
    char vote_id_hex[VOTE_ID_SIZE * 2 + 1];
    for (int i = 0; i < VOTE_ID_SIZE; i++) {
        snprintf(vote_id_hex + i * 2, 3, "%02x", receipt->vote_id[i]);
    }
    
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), 
        "{\"election_id\": \"%s\", \"vote_id\": \"%s\", \"status\": %u, \"timestamp\": %lu}",
        election_id, vote_id_hex, receipt->status, (unsigned long)receipt->timestamp);
    
    http_response_data_t response;
    enclave_result_t result = http_post(endpoint, json_data, &response);
    
    if (response.data) {
        free(response.data);
    }
    
    if (result == ENCLAVE_SUCCESS) {
        log_info("Successfully submitted vote result for election: %s", election_id);
    }
    
    return result;
}

// Store enclave key externally
/* 
// Store enclave key to external storage - Temporarily disabled due to crypto_key_t struct issues
enclave_result_t api_store_enclave_key(const char* key_id, const crypto_key_t* key) {
    // Implementation temporarily disabled
    return ENCLAVE_ERROR_NOT_IMPLEMENTED;
}

// Fetch enclave key from external storage - Temporarily disabled due to crypto_key_t struct issues  
enclave_result_t api_fetch_enclave_key(const char* key_id, crypto_key_t* key) {
    // Implementation temporarily disabled
    return ENCLAVE_ERROR_NOT_IMPLEMENTED;
}
*/

// Store aggregation result externally
enclave_result_t api_store_aggregation_result(const char* election_id, const vote_aggregation_t* result) {
    if (!election_id || !result) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/results/store");
    
    char json_data[2048];
    snprintf(json_data, sizeof(json_data),
        "{\"election_id\": \"%s\", \"total_votes\": %u, \"candidate_votes\": [",
        election_id, result->total_votes);
    
    size_t len = strlen(json_data);
    for (int i = 0; i < MAX_CANDIDATES && len < sizeof(json_data) - 64; i++) {
        if (i > 0) {
            len += snprintf(json_data + len, sizeof(json_data) - len, ", ");
        }
        len += snprintf(json_data + len, sizeof(json_data) - len, "%u", result->candidate_votes[i]);
    }
    
    snprintf(json_data + len, sizeof(json_data) - len, "]}");
    
    http_response_data_t response;
    enclave_result_t result_code = http_post(endpoint, json_data, &response);
    
    if (response.data) {
        free(response.data);
    }
    
    if (result_code == ENCLAVE_SUCCESS) {
        log_info("Successfully stored aggregation result for election: %s", election_id);
    }
    
    return result_code;
}

// Fetch aggregation result from external storage
enclave_result_t api_fetch_aggregation_result(const char* election_id, vote_aggregation_t* result) {
    if (!election_id || !result) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/results/fetch?election_id=%s", election_id);
    
    http_response_data_t response;
    enclave_result_t result_code = http_get(endpoint, &response);
    
    if (result_code != ENCLAVE_SUCCESS) {
        return result_code;
    }
    
    // Mock result data (in real implementation, parse JSON)
    result->total_votes = 10;
    result->candidate_votes[0] = 4;
    result->candidate_votes[1] = 3;
    result->candidate_votes[2] = 3;
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully fetched aggregation result for election: %s", election_id);
    return ENCLAVE_SUCCESS;
}

// Free auxiliary values array
void api_free_auxiliary_values(auxiliary_value_t* values, size_t count) {
    (void)count; // Unused parameter
    if (values) {
        free(values);
    }
}

// Get election parameters from external API
enclave_result_t api_get_election_parameters(election_params_t* params) {
    if (!params) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Fetching election parameters from API");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/election/parameters", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch election parameters");
        return result;
    }
    
    // Mock election parameters (in real implementation, parse JSON response)
    params->num_candidates = 3;
    params->max_votes = 1000;
    params->start_time = 1640995200; // 2022-01-01 00:00:00 UTC
    params->end_time = 1672531200;   // 2023-01-01 00:00:00 UTC
    params->public_key_size = 32;
    strcpy(params->election_name, "Test Election 2024");
    
    // Generate dummy public key
    for (size_t i = 0; i < params->public_key_size; i++) {
        params->public_key[i] = (uint8_t)(0x10 + (i % 16));
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully fetched election parameters");
    return ENCLAVE_SUCCESS;
}

// Get auxiliary values from external API
enclave_result_t api_get_auxiliary_values(auxiliary_values_t* aux_values) {
    if (!aux_values) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Fetching auxiliary values from API");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/auxiliary/values", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values");
        return result;
    }
    
    // Mock auxiliary values (in real implementation, parse JSON response)
    strcpy(aux_values->voter_id, "voter_12345");
    aux_values->aux_value_size = 64;
    aux_values->timestamp = (uint64_t)time(NULL);
    
    // Generate dummy auxiliary value
    for (size_t i = 0; i < aux_values->aux_value_size; i++) {
        aux_values->aux_value[i] = (uint8_t)(0x20 + (i % 16));
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully fetched auxiliary values");
    return ENCLAVE_SUCCESS;
}

// Store vote receipt to external API
enclave_result_t api_store_vote_receipt(const vote_receipt_t* receipt) {
    if (!receipt) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Storing vote receipt to API");
    
    // Create JSON payload for receipt
    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload),
        "{"
        "\"vote_id\": \"");
    
    // Add vote ID as hex string
    size_t pos = strlen(json_payload);
    for (int i = 0; i < VOTE_ID_SIZE && pos < sizeof(json_payload) - 64; i++) {
        pos += snprintf(json_payload + pos, sizeof(json_payload) - pos, 
            "%02x", receipt->vote_id[i]);
    }
    
    snprintf(json_payload + pos, sizeof(json_payload) - pos,
        "\","
        "\"timestamp\": %lu,"
        "\"status\": %d"
        "}", receipt->timestamp, receipt->status);
    
    http_response_data_t response = {0};
    enclave_result_t result = http_post("/api/votes/receipt", json_payload, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store vote receipt");
        return result;
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully stored vote receipt");
    return ENCLAVE_SUCCESS;
}

// Store final results to external API
enclave_result_t api_store_final_results(const final_results_t* results) {
    if (!results) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Storing final results to API for election: %s", results->election_id);
    
    // Create JSON payload for results
    char json_payload[2048];
    snprintf(json_payload, sizeof(json_payload),
        "{"
        "\"election_id\": \"%s\","
        "\"total_votes\": %u,"
        "\"timestamp\": %lu,"
        "\"candidates\": [",
        results->election_id, results->total_votes, results->timestamp);
    
    size_t pos = strlen(json_payload);
    for (uint32_t i = 0; i < results->candidate_count && pos < sizeof(json_payload) - 128; i++) {
        if (i > 0) {
            pos += snprintf(json_payload + pos, sizeof(json_payload) - pos, ",");
        }
        pos += snprintf(json_payload + pos, sizeof(json_payload) - pos,
            "{\"candidate_id\": %u, \"votes\": %u}", 
            i, results->candidate_votes[i]);
    }
    
    snprintf(json_payload + pos, sizeof(json_payload) - pos, "]}");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_post("/api/results/final", json_payload, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store final results");
        return result;
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully stored final results for election: %s", results->election_id);
    return ENCLAVE_SUCCESS;
}

// Get keys from external storage
enclave_result_t api_get_keys(key_pair_t* keys) {
    if (!keys) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Fetching keys from external storage");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/keys/current", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_debug("No existing keys found in external storage");
        return ENCLAVE_ERROR_KEY_NOT_FOUND;
    }
    
    // Mock key loading (in real implementation, parse JSON and decode keys)
    keys->public_key_size = 32;
    keys->private_key_size = 32;
    
    // Generate dummy keys
    for (size_t i = 0; i < keys->public_key_size; i++) {
        keys->public_key[i] = (uint8_t)(0x30 + (i % 16));
    }
    for (size_t i = 0; i < keys->private_key_size; i++) {
        keys->private_key[i] = (uint8_t)(0x40 + (i % 16));
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully fetched keys from external storage");
    return ENCLAVE_SUCCESS;
}

// Store keys to external storage
enclave_result_t api_store_keys(const key_pair_t* keys) {
    if (!keys) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Storing keys to external storage");
    
    // Create JSON payload for keys (in real implementation, encrypt keys first)
    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload),
        "{"
        "\"public_key_size\": %zu,"
        "\"private_key_size\": %zu,"
        "\"timestamp\": %lu"
        "}", keys->public_key_size, keys->private_key_size, (uint64_t)time(NULL));
    
    http_response_data_t response = {0};
    enclave_result_t result = http_post("/api/keys/store", json_payload, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store keys to external storage");
        return result;
    }
    
    if (response.data) {
        free(response.data);
    }
    
    log_info("Successfully stored keys to external storage");
    return ENCLAVE_SUCCESS;
}
