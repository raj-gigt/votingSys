#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "api_client.h"
#include "logging.h"
#include "shared_types.h"
#include "error_codes.h"
#include "secure_crypto_interface.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Global state
static int g_running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    printf("Received signal %d, shutting down gracefully...\n", signal);
    g_running = 0;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Collector Host - Auxiliary Aggregation Only\n");
    printf("Options:\n");
    printf("  -u, --url <url>         Set API base URL (default: http://localhost:3000)\n");
    printf("  -l, --log-level <level> Set log level (0-4)\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
}

// Parse command line arguments
int parse_arguments(int argc, char* argv[], api_config_t* config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--url") == 0) {
            if (i + 1 < argc) {
                strncpy(config->base_url, argv[++i], sizeof(config->base_url) - 1);
            } else {
                fprintf(stderr, "URL option requires a value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 < argc) {
                int log_level = atoi(argv[++i]);
                if (log_level < 0 || log_level > 4) {
                    fprintf(stderr, "Invalid log level: %d\n", log_level);
                    return -1;
                }
                // Set log level (assuming logging supports this)
            } else {
                fprintf(stderr, "Log level option requires a value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1; // Not an error, just exit
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Enclave Collector Host v1.0.0 - Auxiliary Aggregation\n");
            return 1; // Not an error, just exit
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

// Initialize API configuration with defaults
void init_default_config(api_config_t* config) {
    memset(config, 0, sizeof(api_config_t));
    strcpy(config->base_url, "http://localhost:3000");
    config->timeout_ms = 30000; // 30 seconds
    config->max_retries = 3;
}

// Main auxiliary aggregation function
int run_auxiliary_aggregation(void) {
    log_info("Starting auxiliary aggregation process...");
    
    // 1. Initialize by fetching system parameters from API
    crypto_params_t crypto_params;
    enclave_result_t result = api_fetch_system_params(&crypto_params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch system parameters: %d", result);
        return -1;
    }
    
    log_info("System parameters fetched successfully");
    
    // 2. Fetch auxiliary values
    api_auxiliary_value_t* aux_values = NULL;
    size_t count = 0;
    
    result = api_fetch_auxiliary_values(&aux_values, &count);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values: %d", result);
        return -1;
    }
    
    if (count == 0) {
        log_info("No auxiliary values to aggregate");
        return 0;
    }
      log_info("Fetched %zu auxiliary values for aggregation", count);
    
    // 3. Initialize secure crypto processor with system parameters
    result = secure_crypto_init(crypto_params.election_id, 
                               crypto_params.N, 
                               crypto_params.H, 
                               crypto_params.skA);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize secure crypto processor: %d", result);
        api_free_auxiliary_values(aux_values, count);
        return -1;
    }
    
    // 4. Convert auxiliary values to format expected by enclave
    const char** aux_hex_values = malloc(count * sizeof(char*));
    const char** voter_ids = malloc(count * sizeof(char*));
    uint64_t* timestamps = malloc(count * sizeof(uint64_t));
    
    if (!aux_hex_values || !voter_ids || !timestamps) {
        log_error("Failed to allocate memory for auxiliary value arrays");
        free(aux_hex_values);
        free(voter_ids);
        free(timestamps);
        api_free_auxiliary_values(aux_values, count);
        secure_crypto_cleanup();
        return -1;
    }
    
    // Prepare data for batch processing
    for (size_t i = 0; i < count; i++) {
        aux_hex_values[i] = aux_values[i].aux_value;
        voter_ids[i] = aux_values[i].voter_id;
        timestamps[i] = time(NULL); // Use current timestamp if not available
    }
    
    // 5. Process auxiliary values through secure enclave
    int processed_count = 0;
    result = secure_process_auxiliary_values_batch(aux_hex_values, voter_ids, 
                                                  timestamps, count, &processed_count);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to process auxiliary values in enclave: %d", result);
        free(aux_hex_values);
        free(voter_ids);
        free(timestamps);
        api_free_auxiliary_values(aux_values, count);
        secure_crypto_cleanup();
        return -1;
    }
    
    log_info("Successfully processed %d out of %zu auxiliary values", processed_count, count);
    
    // 6. Compute final aggregated result using secure enclave
    char aggregated_result[8192];
    size_t result_size = sizeof(aggregated_result);
    uint8_t zk_proof[2048];
    size_t proof_size = sizeof(zk_proof);
    
    result = secure_compute_final_aggregation(aggregated_result, &result_size, 
                                            zk_proof, &proof_size);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to compute final aggregation: %d", result);
        free(aux_hex_values);
        free(voter_ids);
        free(timestamps);
        api_free_auxiliary_values(aux_values, count);
        secure_crypto_cleanup();
        return -1;
    }
    
    log_info("Secure aggregation completed. Result size: %zu bytes, Proof size: %zu bytes", 
             result_size, proof_size);
    
    // Clean up temporary arrays
    free(aux_hex_values);
    free(voter_ids);
    free(timestamps);// 4. Submit aggregated result
    result = api_submit_aux_product(aggregated_result);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to submit auxiliary product: %d", result);
        api_free_auxiliary_values(aux_values, count);
        return -1;
    }
      log_info("Successfully completed auxiliary aggregation");
    
    // Cleanup secure crypto processor
    secure_crypto_cleanup();
    
    // Cleanup
    api_free_auxiliary_values(aux_values, count);
    return 0;
}

int main(int argc, char* argv[]) {
    int result = 0;
    
    printf("Enclave Collector Host - Auxiliary Aggregation v1.0.0\n");
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize API configuration
    api_config_t api_config;
    init_default_config(&api_config);
    
    // Parse command line arguments
    result = parse_arguments(argc, argv, &api_config);
    if (result != 0) {
        if (result == 1) return 0; // Help or version printed        return result;
    }
    
    // Initialize logging with default config
    host_config_t log_config = {0};
    log_config.log_level = LOG_LEVEL_INFO;  // Default to INFO level
    strcpy(log_config.log_file, "logs/auxiliary_collector.log");  // Default log file
    
    logging_init(&log_config);
    
    log_info("=== Enclave Collector Host - Auxiliary Aggregation ===");
    log_info("Configuration:");
    log_info("  API Base URL: %s", api_config.base_url);
    log_info("  Timeout: %d ms", api_config.timeout_ms);
    log_info("  Max Retries: %d", api_config.max_retries);
    
    // Initialize API client
    enclave_result_t init_result = api_client_init(&api_config);
    if (init_result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize API client: %d", init_result);
        return -1;
    }    // Initialize auxiliary management (simplified - no complex state management needed)
    log_info("Auxiliary management initialized (simplified mode)");
    
    // Run auxiliary aggregation
    result = run_auxiliary_aggregation();    // Cleanup
    log_info("Shutting down auxiliary collector host...");
    api_client_cleanup();
    logging_cleanup();
    
    printf("Enclave Collector Host Stopped. Exit code: %d\n", result);
    return result;
}
