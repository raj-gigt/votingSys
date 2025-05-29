#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "host_interface.h"
#include "logging.h"
#include "network_interface.h"
#include "file_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include "constants.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

// Global state
static int g_running = 1;
static host_context_t g_host_context = {0};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    printf("Received signal %d, shutting down gracefully...\n", signal);
    g_running = 0;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p, --port <port>       Set listening port (default: %d)\n", DEFAULT_PORT);
    printf("  -c, --config <file>     Configuration file path\n");
    printf("  -l, --log-level <level> Set log level (0-4)\n");
    printf("  -s, --simulation        Run in simulation mode\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
}

// Parse command line arguments
int parse_arguments(int argc, char* argv[], host_config_t* config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                config->port = atoi(argv[++i]);
                if (config->port <= 0 || config->port > 65535) {
                    fprintf(stderr, "Invalid port number: %d\n", config->port);
                    return ERROR_INVALID_PARAMETER;
                }
            } else {
                fprintf(stderr, "Port option requires a value\n");
                return ERROR_INVALID_PARAMETER;
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                strncpy(config->config_file, argv[++i], sizeof(config->config_file) - 1);
            } else {
                fprintf(stderr, "Config option requires a file path\n");
                return ERROR_INVALID_PARAMETER;
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 < argc) {
                config->log_level = atoi(argv[++i]);
                if (config->log_level < 0 || config->log_level > 4) {
                    fprintf(stderr, "Invalid log level: %d\n", config->log_level);
                    return ERROR_INVALID_PARAMETER;
                }
            } else {
                fprintf(stderr, "Log level option requires a value\n");
                return ERROR_INVALID_PARAMETER;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--simulation") == 0) {
            config->simulation_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1; // Not an error, just exit
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Enclave Collector Host v1.0.0\n");
            return 1; // Not an error, just exit
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return ERROR_INVALID_PARAMETER;
        }
    }
    return SUCCESS;
}

// Initialize host configuration with defaults
void init_default_config(host_config_t* config) {
    memset(config, 0, sizeof(host_config_t));
    config->port = DEFAULT_PORT;
    config->log_level = 2; // INFO level
    config->max_connections = MAX_CONNECTIONS;
    config->network_timeout = NETWORK_TIMEOUT_MS;
    config->simulation_mode = 1; // Default to simulation mode
    strcpy(config->config_file, "config/enclave.conf");
    strcpy(config->log_file, "logs/collector.log");
}

// Load configuration from file
int load_config_file(const char* filename, host_config_t* config) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        log_warning("Config file not found: %s, using defaults", filename);
        return SUCCESS; // Not an error, use defaults
    }

    char line[512];
    char key[128], value[256];
    
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        if (sscanf(line, "%127[^=]=%255s", key, value) == 2) {
            // Trim whitespace
            char* key_trim = key;
            while (*key_trim == ' ' || *key_trim == '\t') key_trim++;
            char* key_end = key_trim + strlen(key_trim) - 1;
            while (key_end > key_trim && (*key_end == ' ' || *key_end == '\t' || *key_end == '\n' || *key_end == '\r')) {
                *key_end-- = '\0';
            }
            
            if (strcmp(key_trim, "port") == 0) {
                config->port = atoi(value);
            } else if (strcmp(key_trim, "log_level") == 0) {
                config->log_level = atoi(value);
            } else if (strcmp(key_trim, "max_connections") == 0) {
                config->max_connections = atoi(value);
            } else if (strcmp(key_trim, "simulation_mode") == 0) {
                config->simulation_mode = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key_trim, "log_file") == 0) {
                strncpy(config->log_file, value, sizeof(config->log_file) - 1);
            }
        }
    }
    
    fclose(file);
    return SUCCESS;
}

// Main application loop
int run_collector_service(host_context_t* context) {
    log_info("Starting collector service on port %d", context->config.port);

    // Initialize network server
    network_config_t net_config;
    strncpy(net_config.host, "0.0.0.0", sizeof(net_config.host) - 1);
    net_config.host[sizeof(net_config.host) - 1] = '\0';
    net_config.port = context->config.port;
    net_config.max_connections = context->config.max_connections;
    net_config.timeout_seconds = 30;
    net_config.enable_ssl = false;

    network_server_t server;
    enclave_result_t result = network_initialize(&net_config, &server);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize network: %s", get_error_description(result));
        return result;
    }

    // Start network server
    result = network_start_server(&server);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to start network server: %s", get_error_description(result));
        network_cleanup(&server);
        return result;
    }

    log_info("Collector service is running. Press Ctrl+C to stop.");

    // Main service loop
    while (g_running) {
#ifdef _WIN32
        Sleep(1000); // Sleep 1 second
#else
        sleep(1);
#endif
        
        // Periodic tasks could go here
        // For now, just keep the service alive
    }

    log_info("Stopping collector service...");

    // Stop and cleanup network server
    result = network_stop_server(&server);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to stop network server properly: %s", get_error_description(result));
    }

    network_cleanup(&server);
    
    return SUCCESS;
}

int main(int argc, char* argv[]) {
    int result = SUCCESS;
    
    printf("Enclave Collector Host Starting...\n");
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize default configuration
    init_default_config(&g_host_context.config);
    
    // Parse command line arguments
    result = parse_arguments(argc, argv, &g_host_context.config);
    if (result != SUCCESS) {
        if (result == 1) return 0; // Help or version printed
        return result;
    }
    
    // Load configuration file
    result = load_config_file(g_host_context.config.config_file, &g_host_context.config);
    if (result != SUCCESS) {
        fprintf(stderr, "Failed to load configuration: %s\n", get_error_description(result));
        return result;
    }
    
    // Initialize logging
    result = logging_init(&g_host_context.config);
    if (result != SUCCESS) {
        fprintf(stderr, "Failed to initialize logging: %s\n", get_error_description(result));
        return result;
    }
    
    log_info("=== Enclave Collector Host v1.0.0 ===");
    log_info("Configuration:");
    log_info("  Port: %d", g_host_context.config.port);
    log_info("  Log Level: %d", g_host_context.config.log_level);
    log_info("  Simulation Mode: %s", g_host_context.config.simulation_mode ? "Yes" : "No");
    log_info("  Max Connections: %d", g_host_context.config.max_connections);
    
    // Initialize host interface
    result = host_initialize(&g_host_context);
    if (result != SUCCESS) {
        log_error("Failed to initialize host interface: %s", get_error_description(result));
        goto cleanup;
    }
    
    // Run the collector service
    result = run_collector_service(&g_host_context);
    
cleanup:
    log_info("Shutting down collector host...");
    
    // Cleanup host interface
    host_cleanup(&g_host_context);
    
    // Cleanup logging
    logging_cleanup();
    
    printf("Enclave Collector Host Stopped.\n");
    return result;
}
