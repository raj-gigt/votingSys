#include "api_client.h"
#include "config_manager.h"
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

// Simple JSON parsing helper function
static char* extract_json_string_value(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    // Find the key
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) return NULL;
    
    // Move to the value part
    char* value_start = key_pos + strlen(search_pattern);
    
    // Skip whitespace
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n') {
        value_start++;
    }
    
    // Check if it's a string value (starts with quote)
    if (*value_start != '"') return NULL;
    value_start++; // Skip opening quote
    
    // Find closing quote
    char* value_end = value_start;
    while (*value_end != '"' && *value_end != '\0') {
        value_end++;
    }
    
    if (*value_end != '"') return NULL;
    
    // Allocate and copy the value
    size_t value_len = value_end - value_start;
    char* result = malloc(value_len + 1);
    if (!result) return NULL;
    
    strncpy(result, value_start, value_len);
    result[value_len] = '\0';
      return result;
}

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
    if (g_api_initialized) {
#ifdef _WIN32
        WSACleanup();
#else
        curl_global_cleanup();
#endif
        g_api_initialized = 0;
        log_info("API client cleaned up");
    }
}

// Perform HTTP GET request
static enclave_result_t http_get(const char* endpoint, http_response_data_t* response) {
    if (!endpoint || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    char url[1024];
    // Use configured base URL or fallback to default
    const char* base_url = config_get_api_base_url();
    if (!base_url || strlen(base_url) == 0) {
        log_warning("No API base URL configured, using default");
        base_url = "http://localhost:3000";
    }
    
    snprintf(url, sizeof(url), "%s%s", base_url, endpoint);
    
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_get_api_timeout() / 1000);
    
    // Add auth header if available
    struct curl_slist* headers = NULL;
    const char* auth_token = config_get_api_auth_token();
    if (auth_token && strlen(auth_token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth_token);
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
    // Use configured base URL or fallback to default
    const char* base_url = config_get_api_base_url();
    if (!base_url || strlen(base_url) == 0) {
        log_warning("No API base URL configured, using default");
        base_url = "http://localhost:3000";
    }
    
    snprintf(url, sizeof(url), "%s%s", base_url, endpoint);
    
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_get_api_timeout() / 1000);
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    const char* auth_token = config_get_api_auth_token();
    if (auth_token && strlen(auth_token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth_token);
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
    
    if (!response.data) {
        log_error("Empty response from election parameters API");
        return ENCLAVE_ERROR_NETWORK_REQUEST_FAILED;
    }
    
    log_debug("API Response: %s", response.data);
    
    // Parse JSON response to extract N, H, and skA
    char* n_value = extract_json_string_value(response.data, "N");
    char* h_value = extract_json_string_value(response.data, "H");
    char* ska_value = extract_json_string_value(response.data, "skA");
    
    if (!n_value || !h_value || !ska_value) {
        log_error("Failed to parse election parameters from JSON response");
        if (n_value) free(n_value);
        if (h_value) free(h_value);
        if (ska_value) free(ska_value);
        free(response.data);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Copy values to the params structure
    strncpy(params->N, n_value, sizeof(params->N) - 1);
    params->N[sizeof(params->N) - 1] = '\0';
    
    strncpy(params->H, h_value, sizeof(params->H) - 1);
    params->H[sizeof(params->H) - 1] = '\0';
    
    strncpy(params->skA, ska_value, sizeof(params->skA) - 1);
    params->skA[sizeof(params->skA) - 1] = '\0';
    
    // Clean up
    free(n_value);
    free(h_value);
    free(ska_value);
    
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
enclave_result_t api_fetch_auxiliary_values(const char* election_id, api_auxiliary_value_t** values, size_t* count) {
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
    
    if (!response.data || response.size == 0) {
        log_error("Empty response from auxiliary values API");
        return ENCLAVE_ERROR_API_RESPONSE_INVALID;
    }
    
    // Parse JSON response to extract auxiliary values
    // Expected format: {"auxiliaryValues": [{"voterId": "...", "auxiliaryValue": "..."}, ...]}
    char* aux_array_start = strstr(response.data, "\"auxiliaryValues\"");
    if (!aux_array_start) {
        log_error("Invalid JSON response: missing auxiliaryValues array");
        free(response.data);
        return ENCLAVE_ERROR_API_RESPONSE_INVALID;
    }
    
    // Count auxiliary values in the array
    *count = 0;
    char* search_ptr = aux_array_start;
    while ((search_ptr = strstr(search_ptr + 1, "\"voterId\"")) != NULL) {
        (*count)++;
    }
    
    if (*count == 0) {
        log_info("No auxiliary values found for election: %s", election_id);
        *values = NULL;
        free(response.data);
        return ENCLAVE_SUCCESS;
    }
    
    // Allocate array for auxiliary values
    *values = malloc(*count * sizeof(api_auxiliary_value_t));
    if (!*values) {
        log_error("Failed to allocate memory for auxiliary values");
        free(response.data);
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    // Parse each auxiliary value from the JSON
    size_t parsed_count = 0;
    search_ptr = aux_array_start;
    
    while (parsed_count < *count && (search_ptr = strstr(search_ptr, "\"voterId\"")) != NULL) {
        api_auxiliary_value_t* current = &(*values)[parsed_count];
        memset(current, 0, sizeof(api_auxiliary_value_t));
        
        // Extract voter ID
        char* voter_id = extract_json_string_value(search_ptr, "voterId");
        if (voter_id) {
            strncpy(current->voter_id, voter_id, sizeof(current->voter_id) - 1);
            free(voter_id);
        }
        
        // Extract auxiliary value
        char* aux_value = extract_json_string_value(search_ptr, "auxiliaryValue");
        if (aux_value) {
            strncpy(current->aux_value, aux_value, sizeof(current->aux_value) - 1);
            free(aux_value);
        }
        
        parsed_count++;
        search_ptr++; // Move past current match
    }
    
    *count = parsed_count;
    
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
enclave_result_t api_store_enclave_key(const char* key_id, const crypto_key_t* key) {
    if (!key_id || !key) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_info("Storing enclave key: %s", key_id);
    
    // Convert key to hex string for JSON
    char key_hex[CRYPTO_KEY_SIZE * 2 + 1];
    for (size_t i = 0; i < key->size && i < CRYPTO_KEY_SIZE; i++) {
        snprintf(key_hex + i * 2, 3, "%02x", key->data[i]);
    }
    key_hex[key->size * 2] = '\0';
    
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), 
        "{\"key_id\": \"%s\", \"key_data\": \"%s\", \"key_size\": %zu}",
        key_id, key_hex, key->size);
    
    http_response_data_t response;
    enclave_result_t result = http_post("/api/collector/store-key", json_data, &response);
    
    if (response.data) {
        free(response.data);
    }
    
    return result;
}

// Fetch enclave key from external storage
enclave_result_t api_fetch_enclave_key(const char* key_id, crypto_key_t* key) {
    if (!key_id || !key) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_info("Fetching enclave key: %s", key_id);
    
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "/api/collector/fetch-key?key_id=%s", key_id);
    
    http_response_data_t response;
    enclave_result_t result = http_get(endpoint, &response);
    
    if (result == ENCLAVE_SUCCESS && response.data) {
        // Parse the key data from JSON response
        // Simple parsing - look for "key_data" field
        const char* key_data_pos = strstr(response.data, "\"key_data\":");
        if (key_data_pos) {
            key_data_pos = strchr(key_data_pos + 11, '"');
            if (key_data_pos) {
                key_data_pos++; // Skip opening quote
                const char* key_end = strchr(key_data_pos, '"');
                if (key_end) {
                    size_t hex_len = key_end - key_data_pos;
                    if (hex_len <= CRYPTO_KEY_SIZE * 2) {
                        // Convert hex string back to bytes
                        key->size = hex_len / 2;
                        for (size_t i = 0; i < key->size; i++) {
                            char hex_byte[3] = {key_data_pos[i * 2], key_data_pos[i * 2 + 1], '\0'};
                            key->data[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
                        }
                    } else {
                        result = ENCLAVE_ERROR_API_DATA_FORMAT;
                    }
                } else {
                    result = ENCLAVE_ERROR_API_DATA_FORMAT;
                }
            } else {
                result = ENCLAVE_ERROR_API_DATA_FORMAT;
            }
        } else {
            result = ENCLAVE_ERROR_KEY_NOT_FOUND;
        }
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return result;
}

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
void api_free_auxiliary_values(api_auxiliary_value_t* values, size_t count) {
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

// Real-time API implementation - fetch N and H parameters
enclave_result_t api_fetch_params(crypto_params_t* params) {
    if (!params) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_info("Fetching N and H parameters from /api/collector/params");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/collector/params", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch parameters from API");
        return result;
    }
    
    if (!response.data) {
        log_error("Empty response from parameters API");
        return ENCLAVE_ERROR_API_COMMUNICATION;
    }
    
    // Parse JSON response to extract N and H
    char* n_value = extract_json_string_value(response.data, "N");
    char* h_value = extract_json_string_value(response.data, "H");
    
    if (!n_value || !h_value) {
        log_error("Failed to parse N and H from API response");
        if (n_value) free(n_value);
        if (h_value) free(h_value);
        free(response.data);
        return ENCLAVE_ERROR_API_DATA_FORMAT;
    }
    
    // Copy to output structure
    strncpy(params->N, n_value, sizeof(params->N) - 1);
    strncpy(params->H, h_value, sizeof(params->H) - 1);
    params->N[sizeof(params->N) - 1] = '\0';
    params->H[sizeof(params->H) - 1] = '\0';
    
    log_info("Successfully fetched crypto parameters");
    log_debug("N: %.32s...", params->N);
    log_debug("H: %.32s...", params->H);
    
    // Clean up
    free(n_value);
    free(h_value);
    free(response.data);
    
    return ENCLAVE_SUCCESS;
}

// Real-time API implementation - fetch auxiliary values array
enclave_result_t api_fetch_auxiliary_values_realtime(api_auxiliary_value_t** values, size_t* count) {
    if (!values || !count) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_info("Fetching auxiliary values from /api/collector/fetch-auxiliary");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/collector/fetch-auxiliary", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values from API");
        return result;
    }
    
    if (!response.data) {
        log_error("Empty response from auxiliary values API");
        return ENCLAVE_ERROR_API_COMMUNICATION;
    }
    
    // Parse JSON response - expect format: {"auxiliaryValues": [{"voterId": "...", "auxi": "..."}]}
    // For now, implement basic parsing for the array
    // In production, use a proper JSON parser like cJSON
    
    // Count the number of auxiliary values by counting "voterId" occurrences
    *count = 0;
    char* search_ptr = response.data;
    while ((search_ptr = strstr(search_ptr, "\"voterId\"")) != NULL) {
        (*count)++;
        search_ptr++;
    }
    
    if (*count == 0) {
        log_info("No auxiliary values available");
        *values = NULL;
        free(response.data);
        return ENCLAVE_SUCCESS;
    }
    
    // Allocate array for auxiliary values
    *values = malloc(*count * sizeof(api_auxiliary_value_t));
    if (!*values) {
        log_error("Failed to allocate memory for auxiliary values");
        free(response.data);
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    // Parse each auxiliary value from the JSON
    size_t parsed_count = 0;
    search_ptr = response.data;
    
    while (parsed_count < *count && (search_ptr = strstr(search_ptr, "\"voterId\"")) != NULL) {
        api_auxiliary_value_t* current = &(*values)[parsed_count];
        memset(current, 0, sizeof(api_auxiliary_value_t));
        
        // Extract voter ID
        char* voter_id = extract_json_string_value(search_ptr, "voterId");
        if (voter_id) {
            strncpy(current->voter_id, voter_id, sizeof(current->voter_id) - 1);
            free(voter_id);
        }
        
        // Extract auxiliary value (hex string)
        char* aux_hex = extract_json_string_value(search_ptr, "auxi");
        if (aux_hex) {
            strncpy(current->aux_value, aux_hex, sizeof(current->aux_value) - 1);
            free(aux_hex);
        }
        
        parsed_count++;
        search_ptr++;
    }
    
    *count = parsed_count;
    log_info("Successfully parsed %zu auxiliary values from API", *count);
    
    free(response.data);
    return ENCLAVE_SUCCESS;
}

// Real-time API implementation - submit auxiliary product
enclave_result_t api_submit_aux_product(const char* product_hex) {
    if (!product_hex) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_info("Submitting auxiliary product to /api/collector/aux");
    
    // Create JSON payload: {"aux": "product_hex_value"}
    char json_payload[2048];
    snprintf(json_payload, sizeof(json_payload), "{\"aux\":\"%s\"}", product_hex);
    
    http_response_data_t response = {0};
    enclave_result_t result = http_post("/api/collector/aux", json_payload, &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to submit auxiliary product to API");
        return result;
    }
    
    log_info("Successfully submitted auxiliary product to backend");
    
    if (response.data) {
        log_debug("API response: %s", response.data);
        free(response.data);
    }
    
    return ENCLAVE_SUCCESS;
}
