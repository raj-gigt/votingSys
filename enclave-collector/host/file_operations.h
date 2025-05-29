#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "host_interface.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// File operation functions
int file_operations_init(const host_config_t* config);
void file_operations_cleanup(void);

// Basic file operations
int file_read(const char* filename, uint8_t* buffer, size_t* buffer_size);
int file_write(const char* filename, const uint8_t* data, size_t data_size);
int file_append(const char* filename, const uint8_t* data, size_t data_size);
int file_delete(const char* filename);
int file_exists_internal(const char* filename, int* exists);

// Secure file operations for sealed data
int file_write_sealed_data(const char* filename, const sealed_data_t* sealed_data);
int file_read_sealed_data(const char* filename, sealed_data_t* sealed_data);

// Backup and recovery
int file_backup_state(const uint8_t* state_data, size_t state_size, const char* backup_id);
int file_restore_state(const char* backup_id, uint8_t* state_data, size_t* buffer_size);

// Directory operations
int file_create_directory(const char* path);
int file_list_directory(const char* path, char*** file_list, int* file_count);
void file_free_directory_list(char** file_list, int file_count);

// File information
int file_get_size(const char* filename, size_t* file_size);
int file_get_modification_time(const char* filename, uint64_t* mod_time);

// New enclave_result_t-based file operations for integration tests
enclave_result_t write_file_data(const char* filename, const uint8_t* data, size_t data_size);
enclave_result_t read_file_data(const char* filename, uint8_t** data, size_t* data_size);
enclave_result_t delete_file(const char* filename);
bool file_exists(const char* filename);
enclave_result_t create_file_backup(const char* source_filename, const char* backup_filename);
enclave_result_t write_sealed_data(const char* filename, const sealed_data_t* sealed_data, const uint8_t* data);
enclave_result_t read_sealed_data(const char* filename, sealed_data_t* sealed_data, uint8_t** data);

#ifdef __cplusplus
}
#endif

#endif // FILE_OPERATIONS_H
