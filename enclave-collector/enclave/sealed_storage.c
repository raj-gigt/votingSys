#include "enclave_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include "constants.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Sealed storage state
static uint8_t* sealed_storage_buffer = NULL;
static size_t sealed_storage_size = 0;
static bool sealed_storage_initialized = false;

// Initialize sealed storage
static enclave_result_t initialize_sealed_storage(void) {
    if (sealed_storage_initialized) {
        return ENCLAVE_SUCCESS;
    }

    // In simulation mode, use regular memory
    sealed_storage_buffer = (uint8_t*)calloc(MAX_SEALED_DATA_SIZE, sizeof(uint8_t));
    if (!sealed_storage_buffer) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    sealed_storage_size = 0;
    sealed_storage_initialized = true;
    return ENCLAVE_SUCCESS;
}

// Cleanup sealed storage
static void cleanup_sealed_storage(void) {
    if (sealed_storage_buffer) {
        free(sealed_storage_buffer);
        sealed_storage_buffer = NULL;
    }
    sealed_storage_size = 0;
    sealed_storage_initialized = false;
}

// Seal data (encrypt and authenticate)
enclave_result_t seal_data(const uint8_t* data, size_t data_len, 
                          uint8_t** sealed_data, size_t* sealed_len) {
    if (!data || !sealed_data || !sealed_len || data_len == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Initialize sealed storage if needed
    enclave_result_t result = initialize_sealed_storage();
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }

    // In simulation mode, add a simple header and "encrypt" with XOR
    // In production, use SGX sealing APIs
    
    // Calculate sealed data size (header + encrypted data + MAC)
    size_t header_size = sizeof(uint32_t) + sizeof(size_t); // magic + original_size
    size_t mac_size = 16; // 128-bit MAC
    *sealed_len = header_size + data_len + mac_size;

    *sealed_data = (uint8_t*)malloc(*sealed_len);
    if (!*sealed_data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    uint8_t* ptr = *sealed_data;

    // Write header
    uint32_t magic = 0x5EA1ED; // "SEALED" in hex
    memcpy(ptr, &magic, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &data_len, sizeof(size_t));
    ptr += sizeof(size_t);

    // "Encrypt" data with simple XOR (simulation only)
    for (size_t i = 0; i < data_len; i++) {
        ptr[i] = data[i] ^ (uint8_t)(0xAA ^ (i & 0xFF));
    }
    ptr += data_len;

    // Generate simple MAC (simulation only)
    uint32_t mac_high = 0xDEADBEEF;
    uint32_t mac_low = 0xCAFEBABE;
    memcpy(ptr, &mac_high, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &mac_low, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    // Fill remaining MAC bytes
    for (size_t i = 8; i < mac_size; i++) {
        ptr[i - 8] = (uint8_t)(i & 0xFF);
    }

    return ENCLAVE_SUCCESS;
}

// Unseal data (decrypt and verify)
enclave_result_t unseal_data(const uint8_t* sealed_data, size_t sealed_len,
                            uint8_t** data, size_t* data_len) {
    if (!sealed_data || !data || !data_len || sealed_len == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Check minimum size
    size_t header_size = sizeof(uint32_t) + sizeof(size_t);
    size_t mac_size = 16;
    if (sealed_len < header_size + mac_size) {
        return ENCLAVE_ERROR_INVALID_SEALED_DATA;
    }

    const uint8_t* ptr = sealed_data;

    // Verify header magic
    uint32_t magic;
    memcpy(&magic, ptr, sizeof(uint32_t));
    if (magic != 0x5EA1ED) {
        return ENCLAVE_ERROR_INVALID_SEALED_DATA;
    }
    ptr += sizeof(uint32_t);

    // Read original data length
    memcpy(data_len, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    // Verify sealed data size
    if (sealed_len != header_size + *data_len + mac_size) {
        return ENCLAVE_ERROR_INVALID_SEALED_DATA;
    }

    // Allocate output buffer
    *data = (uint8_t*)malloc(*data_len);
    if (!*data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    // "Decrypt" data (reverse the XOR operation)
    for (size_t i = 0; i < *data_len; i++) {
        (*data)[i] = ptr[i] ^ (uint8_t)(0xAA ^ (i & 0xFF));
    }
    ptr += *data_len;

    // Verify MAC (simple check for simulation)
    uint32_t expected_mac_high = 0xDEADBEEF;
    uint32_t expected_mac_low = 0xCAFEBABE;
    uint32_t actual_mac_high, actual_mac_low;
    
    memcpy(&actual_mac_high, ptr, sizeof(uint32_t));
    memcpy(&actual_mac_low, ptr + sizeof(uint32_t), sizeof(uint32_t));

    if (actual_mac_high != expected_mac_high || actual_mac_low != expected_mac_low) {
        free(*data);
        *data = NULL;
        return ENCLAVE_ERROR_SEALED_DATA_INTEGRITY;
    }

    return ENCLAVE_SUCCESS;
}

// Store sealed data in enclave memory
enclave_result_t store_sealed_data_internal(const uint8_t* sealed_data, size_t sealed_len) {
    if (!sealed_data || sealed_len == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    enclave_result_t result = initialize_sealed_storage();
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }

    if (sealed_len > MAX_SEALED_DATA_SIZE) {
        return ENCLAVE_ERROR_SEALED_DATA_TOO_LARGE;
    }

    memcpy(sealed_storage_buffer, sealed_data, sealed_len);
    sealed_storage_size = sealed_len;

    return ENCLAVE_SUCCESS;
}

// Retrieve sealed data from enclave memory
enclave_result_t retrieve_sealed_data_internal(uint8_t** sealed_data, size_t* sealed_len) {
    if (!sealed_data || !sealed_len) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!sealed_storage_initialized || sealed_storage_size == 0) {
        return ENCLAVE_ERROR_NO_SEALED_DATA;
    }

    *sealed_data = (uint8_t*)malloc(sealed_storage_size);
    if (!*sealed_data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    memcpy(*sealed_data, sealed_storage_buffer, sealed_storage_size);
    *sealed_len = sealed_storage_size;

    return ENCLAVE_SUCCESS;
}

// Clear sealed storage
enclave_result_t clear_sealed_storage(void) {
    if (sealed_storage_initialized && sealed_storage_buffer) {
        memset(sealed_storage_buffer, 0, MAX_SEALED_DATA_SIZE);
        sealed_storage_size = 0;
    }
    return ENCLAVE_SUCCESS;
}

// Get sealed storage info
enclave_result_t get_sealed_storage_info(sealed_storage_info_t* info) {
    if (!info) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    info->is_initialized = sealed_storage_initialized;
    info->current_size = sealed_storage_size;
    info->max_size = MAX_SEALED_DATA_SIZE;
    info->available_size = MAX_SEALED_DATA_SIZE - sealed_storage_size;

    return ENCLAVE_SUCCESS;
}
