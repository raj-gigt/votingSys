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

// Simple HTTP GET function
static enclave_result_t http_get(const char* endpoint, http_response_data_t* response) {
    if (!endpoint || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize response
    response->data = malloc(1);
    response->size = 0;
    
    if (!response->data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
#ifdef _WIN32
    // Simple Windows socket implementation
    // For production, use a proper HTTP client library
    log_warning("Windows HTTP implementation not fully implemented");
    free(response->data);
    response->data = malloc(100);
    strcpy(response->data, "{\"status\":\"mock_response\"}");
    response->size = strlen(response->data);
    return ENCLAVE_SUCCESS;
#else
    // Use libcurl for Unix-like systems
    CURL* curl = curl_easy_init();
    if (!curl) {
        free(response->data);
        return ENCLAVE_ERROR_NETWORK_INIT;
    }
    
    // Build full URL
    char full_url[1024];
    snprintf(full_url, sizeof(full_url), "%s%s", g_api_config.base_url, endpoint);
    
    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_api_config.timeout_ms / 1000);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("HTTP GET failed: %s", curl_easy_strerror(res));
        free(response->data);
        return ENCLAVE_ERROR_NETWORK_REQUEST_FAILED;
    }
#endif
    
    return ENCLAVE_SUCCESS;
}

// Simple HTTP POST function
static enclave_result_t http_post(const char* endpoint, const char* json_data, http_response_data_t* response) {
    if (!endpoint || !json_data || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize response
    response->data = malloc(1);
    response->size = 0;
    
    if (!response->data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
#ifdef _WIN32
    // Simple Windows socket implementation
    log_warning("Windows HTTP POST implementation not fully implemented");
    free(response->data);
    response->data = malloc(100);
    strcpy(response->data, "{\"status\":\"success\"}");
    response->size = strlen(response->data);
    return ENCLAVE_SUCCESS;
#else
    // Use libcurl for Unix-like systems
    CURL* curl = curl_easy_init();
    if (!curl) {
        free(response->data);
        return ENCLAVE_ERROR_NETWORK_INIT;
    }
    
    // Build full URL
    char full_url[1024];
    snprintf(full_url, sizeof(full_url), "%s%s", g_api_config.base_url, endpoint);
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_api_config.timeout_ms / 1000);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("HTTP POST failed: %s", curl_easy_strerror(res));
        free(response->data);
        return ENCLAVE_ERROR_NETWORK_REQUEST_FAILED;
    }
#endif
    
    return ENCLAVE_SUCCESS;
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
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log_error("Failed to initialize Winsock");
        return ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED;
    }
#else
    // Initialize libcurl for Unix-like systems
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        log_error("Failed to initialize libcurl");
        return ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED;
    }
#endif
    
    g_api_initialized = 1;
    log_info("API client initialized successfully");
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
    
    g_api_initialized = 0;
    log_info("API client cleanup completed");
}

// 1. Get system parameters for collector initialization
enclave_result_t api_fetch_system_params(crypto_params_t* params) {
    if (!params) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Fetching system parameters from API");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/collector/params", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch system parameters");
        return result;
    }
    
    if (!response.data || response.size == 0) {
        log_error("Empty response from system parameters API");
        return ENCLAVE_ERROR_API_RESPONSE_INVALID;
    }
    
    // Parse JSON response to extract N, H, skA
    char* n_value = extract_json_string_value(response.data, "N");
    char* h_value = extract_json_string_value(response.data, "H");
    char* ska_value = extract_json_string_value(response.data, "skA");
    
    if (n_value && h_value && ska_value) {
        strncpy(params->N, n_value, sizeof(params->N) - 1);
        strncpy(params->H, h_value, sizeof(params->H) - 1);
        strncpy(params->skA, ska_value, sizeof(params->skA) - 1);
        
        params->N[sizeof(params->N) - 1] = '\0';
        params->H[sizeof(params->H) - 1] = '\0';
        params->skA[sizeof(params->skA) - 1] = '\0';
        
        result = ENCLAVE_SUCCESS;
        log_info("Successfully fetched system parameters");
    } else {
        log_error("Failed to parse system parameters from JSON response");
        result = ENCLAVE_ERROR_API_DATA_FORMAT;
    }
    
    // Cleanup
    if (n_value) free(n_value);
    if (h_value) free(h_value);
    if (ska_value) free(ska_value);
    if (response.data) free(response.data);
    
    return result;
}

// 2. Get all auxiliary values
enclave_result_t api_fetch_auxiliary_values(api_auxiliary_value_t** values, size_t* count) {
    if (!values || !count) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Fetching auxiliary values from API");
    
    http_response_data_t response = {0};
    enclave_result_t result = http_get("/api/collector/fetch-auxiliary", &response);
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values");
        return result;
    }
    
    if (!response.data || response.size == 0) {
        log_error("Empty response from auxiliary values API");
        if (response.data) free(response.data);
        return ENCLAVE_ERROR_API_RESPONSE_INVALID;
    }
    
    // Parse JSON response to extract auxiliary values
    // Expected format: {"auxiliaryValues": [{"voterId": "...", "auxi": "..."}, ...]}
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
        log_info("No auxiliary values found");
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
        
        // Extract auxiliary value (the field is "auxi" in the API response)
        char* aux_value = extract_json_string_value(search_ptr, "auxi");
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
    
    log_info("Fetched %zu auxiliary values", *count);
    return ENCLAVE_SUCCESS;
}

// 3. Submit computed auxiliary product (aux)
enclave_result_t api_submit_aux_product(const char* aux_product) {
    if (!aux_product) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!g_api_initialized) {
        return ENCLAVE_ERROR_API_NOT_INITIALIZED;
    }
    
    log_debug("Submitting auxiliary product to API");
    
    // Create JSON payload
    char json_data[4096];
    snprintf(json_data, sizeof(json_data), "{\"aux\": \"%s\"}", aux_product);
    
    http_response_data_t response = {0};
    enclave_result_t result = http_post("/api/collector/aux", json_data, &response);
    
    if (result == ENCLAVE_SUCCESS) {
        log_info("Successfully submitted auxiliary product");
    } else {
        log_error("Failed to submit auxiliary product");
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return result;
}

// Free auxiliary values array
void api_free_auxiliary_values(api_auxiliary_value_t* values, size_t count) {
    (void)count; // Unused parameter
    if (values) {
        free(values);
    }
}
