#include "config_manager.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Global configuration state
static system_config_t g_system_config = {0};
static int g_config_loaded = 0;

// Forward declarations for internal functions
static char* extract_config_string(const char* json, const char* key);
static int extract_config_int(const char* json, const char* key, int default_value);

// Default configuration values
#define DEFAULT_API_BASE_URL "http://localhost:3000"
#define DEFAULT_API_TIMEOUT_MS 30000
#define DEFAULT_API_MAX_RETRIES 3
#define DEFAULT_LOG_LEVEL "INFO"
#define DEFAULT_DATA_DIRECTORY "./data"

// Load configuration from file (JSON format)
enclave_result_t config_load_from_file(const char* config_file_path, system_config_t* config) {
    if (!config_file_path || !config) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    FILE* file = fopen(config_file_path, "r");
    if (!file) {
        log_warning("Config file not found: %s, using defaults", config_file_path);
        return config_load_default(config);
    }
    
    // Read file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 1024 * 1024) { // Max 1MB config file
        log_error("Invalid config file size: %ld", file_size);
        fclose(file);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t read_size = fread(content, 1, file_size, file);
    content[read_size] = '\0';
    fclose(file);
    
    // Simple JSON parsing for configuration
    // Parse API base URL
    char* api_url = extract_config_string(content, "api_base_url");
    if (api_url) {
        strncpy(config->api_config.base_url, api_url, sizeof(config->api_config.base_url) - 1);
        free(api_url);
    } else {
        strcpy(config->api_config.base_url, DEFAULT_API_BASE_URL);
    }
    
    // Parse API auth token
    char* auth_token = extract_config_string(content, "api_auth_token");
    if (auth_token) {
        strncpy(config->api_config.auth_token, auth_token, sizeof(config->api_config.auth_token) - 1);
        free(auth_token);
    } else {
        config->api_config.auth_token[0] = '\0';
    }
    
    // Parse API timeout
    config->api_config.timeout_ms = extract_config_int(content, "api_timeout_ms", DEFAULT_API_TIMEOUT_MS);
    config->api_config.max_retries = extract_config_int(content, "api_max_retries", DEFAULT_API_MAX_RETRIES);
    
    // Parse log level
    char* log_level = extract_config_string(content, "log_level");
    if (log_level) {
        strncpy(config->log_level, log_level, sizeof(config->log_level) - 1);
        free(log_level);
    } else {
        strcpy(config->log_level, DEFAULT_LOG_LEVEL);
    }
    
    // Parse data directory
    char* data_dir = extract_config_string(content, "data_directory");
    if (data_dir) {
        strncpy(config->data_directory, data_dir, sizeof(config->data_directory) - 1);
        free(data_dir);
    } else {
        strcpy(config->data_directory, DEFAULT_DATA_DIRECTORY);
    }
    
    // Parse security settings
    config->enable_tls = extract_config_int(content, "enable_tls", 1);
    config->enable_auth = extract_config_int(content, "enable_auth", 1);
    
    free(content);
    
    log_info("Configuration loaded from file: %s", config_file_path);
    log_info("API Base URL: %s", config->api_config.base_url);
    log_info("API Timeout: %d ms", config->api_config.timeout_ms);
    log_info("Log Level: %s", config->log_level);
    log_info("Data Directory: %s", config->data_directory);
    
    return ENCLAVE_SUCCESS;
}

// Load configuration from environment variables
enclave_result_t config_load_from_env(system_config_t* config) {
    if (!config) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Start with defaults
    enclave_result_t result = config_load_default(config);
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }
    
    // Override with environment variables
    const char* env_value;
    
    // API configuration from environment
    env_value = getenv("ENCLAVE_API_BASE_URL");
    if (env_value && strlen(env_value) > 0) {
        strncpy(config->api_config.base_url, env_value, sizeof(config->api_config.base_url) - 1);
        log_info("Using API base URL from environment: %s", env_value);
    }
    
    env_value = getenv("ENCLAVE_API_AUTH_TOKEN");
    if (env_value && strlen(env_value) > 0) {
        strncpy(config->api_config.auth_token, env_value, sizeof(config->api_config.auth_token) - 1);
        log_info("Using API auth token from environment");
    }
    
    env_value = getenv("ENCLAVE_API_TIMEOUT_MS");
    if (env_value && strlen(env_value) > 0) {
        int timeout = atoi(env_value);
        if (timeout > 0) {
            config->api_config.timeout_ms = timeout;
            log_info("Using API timeout from environment: %d ms", timeout);
        }
    }
    
    env_value = getenv("ENCLAVE_API_MAX_RETRIES");
    if (env_value && strlen(env_value) > 0) {
        int retries = atoi(env_value);
        if (retries >= 0) {
            config->api_config.max_retries = retries;
            log_info("Using API max retries from environment: %d", retries);
        }
    }
    
    // Log level from environment
    env_value = getenv("ENCLAVE_LOG_LEVEL");
    if (env_value && strlen(env_value) > 0) {
        strncpy(config->log_level, env_value, sizeof(config->log_level) - 1);
        log_info("Using log level from environment: %s", env_value);
    }
    
    // Data directory from environment
    env_value = getenv("ENCLAVE_DATA_DIR");
    if (env_value && strlen(env_value) > 0) {
        strncpy(config->data_directory, env_value, sizeof(config->data_directory) - 1);
        log_info("Using data directory from environment: %s", env_value);
    }
    
    // Security settings from environment
    env_value = getenv("ENCLAVE_ENABLE_TLS");
    if (env_value && strlen(env_value) > 0) {
        config->enable_tls = (strcmp(env_value, "true") == 0 || strcmp(env_value, "1") == 0) ? 1 : 0;
        log_info("Using TLS setting from environment: %s", config->enable_tls ? "enabled" : "disabled");
    }
    
    env_value = getenv("ENCLAVE_ENABLE_AUTH");
    if (env_value && strlen(env_value) > 0) {
        config->enable_auth = (strcmp(env_value, "true") == 0 || strcmp(env_value, "1") == 0) ? 1 : 0;
        log_info("Using auth setting from environment: %s", config->enable_auth ? "enabled" : "disabled");
    }
    
    return ENCLAVE_SUCCESS;
}

// Load default configuration
enclave_result_t config_load_default(system_config_t* config) {
    if (!config) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Clear configuration
    memset(config, 0, sizeof(system_config_t));
    
    // Set API defaults
    strcpy(config->api_config.base_url, DEFAULT_API_BASE_URL);
    config->api_config.auth_token[0] = '\0'; // Empty auth token by default
    config->api_config.timeout_ms = DEFAULT_API_TIMEOUT_MS;
    config->api_config.max_retries = DEFAULT_API_MAX_RETRIES;
    
    // Set other defaults
    strcpy(config->log_level, DEFAULT_LOG_LEVEL);
    strcpy(config->data_directory, DEFAULT_DATA_DIRECTORY);
    config->enable_tls = 1;
    config->enable_auth = 1;
    
    log_info("Default configuration loaded");
    return ENCLAVE_SUCCESS;
}

// Validate configuration
enclave_result_t config_validate(const system_config_t* config) {
    if (!config) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Validate API base URL
    if (strlen(config->api_config.base_url) == 0) {
        log_error("API base URL cannot be empty");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Basic URL validation
    if (strncmp(config->api_config.base_url, "http://", 7) != 0 && 
        strncmp(config->api_config.base_url, "https://", 8) != 0) {
        log_error("API base URL must start with http:// or https://");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Validate timeout
    if (config->api_config.timeout_ms <= 0 || config->api_config.timeout_ms > 300000) { // Max 5 minutes
        log_error("API timeout must be between 1 and 300000 ms");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Validate max retries
    if (config->api_config.max_retries < 0 || config->api_config.max_retries > 10) {
        log_error("API max retries must be between 0 and 10");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    // Validate log level
    if (strcmp(config->log_level, "DEBUG") != 0 && 
        strcmp(config->log_level, "INFO") != 0 && 
        strcmp(config->log_level, "WARNING") != 0 && 
        strcmp(config->log_level, "ERROR") != 0) {
        log_error("Invalid log level: %s", config->log_level);
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    log_info("Configuration validation passed");
    return ENCLAVE_SUCCESS;
}

// Helper function to get current API base URL
const char* config_get_api_base_url(void) {
    if (!g_config_loaded) {
        return DEFAULT_API_BASE_URL;
    }
    return g_system_config.api_config.base_url;
}

// Helper function to get current API auth token
const char* config_get_api_auth_token(void) {
    if (!g_config_loaded) {
        return "";
    }
    return g_system_config.api_config.auth_token;
}

// Helper function to get current API timeout
int config_get_api_timeout(void) {
    if (!g_config_loaded) {
        return DEFAULT_API_TIMEOUT_MS;
    }
    return g_system_config.api_config.timeout_ms;
}

// Simple JSON string extraction helper
static char* extract_config_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) return NULL;
    
    char* value_start = key_pos + strlen(search_pattern);
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n') {
        value_start++;
    }
    
    if (*value_start != '"') return NULL;
    value_start++;
    
    char* value_end = value_start;
    while (*value_end != '"' && *value_end != '\0') {
        value_end++;
    }
    
    if (*value_end != '"') return NULL;
    
    size_t value_len = value_end - value_start;
    char* result = malloc(value_len + 1);
    if (!result) return NULL;
    
    strncpy(result, value_start, value_len);
    result[value_len] = '\0';
    return result;
}

// Simple JSON integer extraction helper
static int extract_config_int(const char* json, const char* key, int default_value) {
    if (!json || !key) return default_value;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) return default_value;
    
    char* value_start = key_pos + strlen(search_pattern);
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n') {
        value_start++;
    }
    
    // Parse integer (not string)
    int value = atoi(value_start);
    return value;
}

// Initialize configuration system
enclave_result_t config_system_init(const char* config_file_path) {
    enclave_result_t result;
    
    // Try to load from file first
    if (config_file_path) {
        result = config_load_from_file(config_file_path, &g_system_config);
        if (result == ENCLAVE_SUCCESS) {
            // Override with environment variables
            config_load_from_env(&g_system_config);
        }
    } else {
        // Load from environment and defaults
        result = config_load_from_env(&g_system_config);
    }
    
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to load configuration");
        return result;
    }
    
    // Validate configuration
    result = config_validate(&g_system_config);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Configuration validation failed");
        return result;
    }
    
    g_config_loaded = 1;
    log_info("Configuration system initialized successfully");
    return ENCLAVE_SUCCESS;
}

// Get current system configuration
const system_config_t* config_get_current(void) {
    if (!g_config_loaded) {
        return NULL;
    }
    return &g_system_config;
}
