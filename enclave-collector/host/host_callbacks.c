#include "host_interface.h"
#include "logging.h"
#include "file_operations.h"
#include "shared_types.h"
#include "error_codes.h"

// OCALL implementations for enclave communication
// These functions are called from the enclave via OCALLs

// OCALL: Write to log
void ocall_write_log(int level, const char* message) {
    if (!message) {
        return;
    }

    switch (level) {
        case 0: // ERROR
            log_error("[ENCLAVE] %s", message);
            break;
        case 1: // WARNING
            log_warning("[ENCLAVE] %s", message);
            break;
        case 2: // INFO
            log_info("[ENCLAVE] %s", message);
            break;
        case 3: // DEBUG
            log_debug("[ENCLAVE] %s", message);
            break;
        default:
            log_info("[ENCLAVE] %s", message);
            break;
    }
}

// OCALL: Write data to file
enclave_result_t ocall_write_file(const char* filename, const uint8_t* data, size_t data_len) {
    if (!filename || !data || data_len == 0) {
        log_error("Invalid parameters for file write");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Writing %zu bytes to file: %s", data_len, filename);
    return write_file_data(filename, data, data_len);
}

// OCALL: Read data from file
enclave_result_t ocall_read_file(const char* filename, uint8_t** data, size_t* data_len) {
    if (!filename || !data || !data_len) {
        log_error("Invalid parameters for file read");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Reading data from file: %s", filename);
    return read_file_data(filename, data, data_len);
}

// OCALL: Check if file exists
bool ocall_file_exists(const char* filename) {
    if (!filename) {
        return false;
    }

    // Adjust the arguments below to match the actual definition of file_exists
    
    return file_exists(filename);
}

// OCALL: Delete file
enclave_result_t ocall_delete_file(const char* filename) {
    if (!filename) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Deleting file: %s", filename);
    return delete_file(filename);
}

// OCALL: Get current timestamp
uint64_t ocall_get_timestamp(void) {
    return get_current_timestamp();
}

// OCALL: Get random bytes
enclave_result_t ocall_get_random(uint8_t* buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Simple random number generation for simulation
    // In production, use a proper cryptographic RNG
    for (size_t i = 0; i < buffer_len; i++) {
        buffer[i] = (uint8_t)(rand() % 256);
    }

    return ENCLAVE_SUCCESS;
}

// OCALL: Network send (placeholder for future networking)
enclave_result_t ocall_network_send(const char* endpoint, const uint8_t* data, size_t data_len) {
    if (!endpoint || !data || data_len == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Network send to %s (%zu bytes) - Not implemented", endpoint, data_len);
    // Placeholder - in a real implementation, this would send data over network
    return ENCLAVE_SUCCESS;
}

// OCALL: Allocate memory (for enclave use)
void* ocall_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    void* ptr = malloc(size);
    if (ptr) {
        log_debug("Allocated %zu bytes at %p for enclave", size, ptr);
    } else {
        log_error("Failed to allocate %zu bytes for enclave", size);
    }
    return ptr;
}

// OCALL: Free memory
void ocall_free(void* ptr) {
    if (ptr) {
        log_debug("Freeing memory at %p", ptr);
        free(ptr);
    }
}

// OCALL: Get system information
enclave_result_t ocall_get_system_info(system_info_t* info) {
    if (!info) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Fill in system information
    strncpy(info->hostname, "localhost", sizeof(info->hostname) - 1);
    info->hostname[sizeof(info->hostname) - 1] = '\0';
    
    info->total_memory = 8 * 1024 * 1024 * 1024ULL; // 8GB placeholder
    info->available_memory = 4 * 1024 * 1024 * 1024ULL; // 4GB placeholder
    info->cpu_count = 4; // Placeholder
    info->uptime = 3600; // 1 hour placeholder

    log_debug("Retrieved system info: hostname=%s, memory=%llu", 
              info->hostname, info->total_memory);

    return ENCLAVE_SUCCESS;
}
