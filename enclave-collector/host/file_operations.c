#include "file_operations.h"
#include "logging.h"
#include "error_codes.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#define R_OK 4
#define W_OK 2
#else
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#endif

// Global file operations state
static struct {
    char base_directory[256];
    char backup_directory[256];
    int initialized;
} g_file_state = {0};

// Initialize file operations
int file_operations_init(const host_config_t* config) {
    if (g_file_state.initialized) {
        return SUCCESS;
    }

    log_info("Initializing file operations");

    // Set base directory
    strcpy(g_file_state.base_directory, "data");
    strcpy(g_file_state.backup_directory, "backups");

    // Create directories if they don't exist
    int result = file_create_directory(g_file_state.base_directory);
    if (result != SUCCESS) {
        log_error("Failed to create base directory: %s", g_file_state.base_directory);
        return result;
    }

    result = file_create_directory(g_file_state.backup_directory);
    if (result != SUCCESS) {
        log_error("Failed to create backup directory: %s", g_file_state.backup_directory);
        return result;
    }

    // Create subdirectories
    result = file_create_directory("data/sealed");
    if (result != SUCCESS && result != ERROR_GENERAL_FAILURE) { // May already exist
        log_warning("Could not create sealed data directory");
    }

    result = file_create_directory("data/votes");
    if (result != SUCCESS && result != ERROR_GENERAL_FAILURE) {
        log_warning("Could not create votes directory");
    }

    result = file_create_directory("logs");
    if (result != SUCCESS && result != ERROR_GENERAL_FAILURE) {
        log_warning("Could not create logs directory");
    }

    g_file_state.initialized = 1;
    log_info("File operations initialized");
    return SUCCESS;
}

// Cleanup file operations
void file_operations_cleanup(void) {
    if (!g_file_state.initialized) {
        return;
    }

    log_info("Cleaning up file operations");
    g_file_state.initialized = 0;
}

// Read file contents
int file_read(const char* filename, uint8_t* buffer, size_t* buffer_size) {
    if (!filename || !buffer || !buffer_size) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Reading file: %s", filename);

    FILE* file = fopen(filename, "rb");
    if (!file) {
        log_warning("Could not open file for reading: %s", filename);
        return ERROR_FILE_NOT_FOUND;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        log_error("Could not determine file size: %s", filename);
        return ERROR_FILE_READ_FAILED;
    }

    if ((size_t)file_size > *buffer_size) {
        fclose(file);
        *buffer_size = (size_t)file_size;
        return ERROR_BUFFER_TOO_SMALL;
    }

    // Read file contents
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        log_error("Failed to read complete file: %s (read %zu of %ld bytes)", 
                  filename, bytes_read, file_size);
        return ERROR_FILE_READ_FAILED;
    }

    *buffer_size = bytes_read;
    log_debug("Successfully read %zu bytes from file: %s", bytes_read, filename);
    return SUCCESS;
}

// Write file contents
int file_write(const char* filename, const uint8_t* data, size_t data_size) {
    if (!filename || !data || data_size == 0) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Writing file: %s (%zu bytes)", filename, data_size);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        log_error("Could not open file for writing: %s", filename);
        return ERROR_FILE_WRITE_FAILED;
    }

    size_t bytes_written = fwrite(data, 1, data_size, file);
    int close_result = fclose(file);

    if (bytes_written != data_size || close_result != 0) {
        log_error("Failed to write complete file: %s (wrote %zu of %zu bytes)", 
                  filename, bytes_written, data_size);
        return ERROR_FILE_WRITE_FAILED;
    }

    log_debug("Successfully wrote %zu bytes to file: %s", bytes_written, filename);
    return SUCCESS;
}

// Append to file
int file_append(const char* filename, const uint8_t* data, size_t data_size) {
    if (!filename || !data || data_size == 0) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Appending to file: %s (%zu bytes)", filename, data_size);

    FILE* file = fopen(filename, "ab");
    if (!file) {
        log_error("Could not open file for appending: %s", filename);
        return ERROR_FILE_WRITE_FAILED;
    }

    size_t bytes_written = fwrite(data, 1, data_size, file);
    int close_result = fclose(file);

    if (bytes_written != data_size || close_result != 0) {
        log_error("Failed to append to file: %s (wrote %zu of %zu bytes)", 
                  filename, bytes_written, data_size);
        return ERROR_FILE_WRITE_FAILED;
    }

    log_debug("Successfully appended %zu bytes to file: %s", bytes_written, filename);
    return SUCCESS;
}

// Delete file
int file_delete(const char* filename) {
    if (!filename) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Deleting file: %s", filename);

    if (remove(filename) != 0) {
        log_warning("Could not delete file: %s", filename);
        return ERROR_FILE_NOT_FOUND;
    }

    log_debug("Successfully deleted file: %s", filename);
    return SUCCESS;
}

// Check if file exists
int file_exists_internal(const char* filename, int* exists) {
    if (!filename || !exists) {
        return ERROR_NULL_POINTER;
    }

    *exists = (access(filename, F_OK) == 0) ? 1 : 0;
    return SUCCESS;
}

// Write sealed data to file
int file_write_sealed_data(const char* filename, const sealed_data_t* sealed_data) {
    if (!filename || !sealed_data) {
        return ERROR_NULL_POINTER;
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "data/sealed/%s", filename);

    log_debug("Writing sealed data to: %s (%zu bytes)", full_path, sealed_data->sealed_data_size);

    return file_write(full_path, sealed_data->sealed_data, sealed_data->sealed_data_size);
}

// Read sealed data from file
int file_read_sealed_data(const char* filename, sealed_data_t* sealed_data) {
    if (!filename || !sealed_data) {
        return ERROR_NULL_POINTER;
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "data/sealed/%s", filename);

    log_debug("Reading sealed data from: %s", full_path);

    size_t buffer_size = MAX_SEALED_DATA_SIZE;
    int result = file_read(full_path, sealed_data->sealed_data, &buffer_size);
    
    if (result == SUCCESS) {
        sealed_data->sealed_data_size = buffer_size;
    }

    return result;
}

// Create directory
int file_create_directory(const char* path) {
    if (!path) {
        return ERROR_NULL_POINTER;
    }

    // Check if directory already exists
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((st.st_mode & S_IFDIR) != 0) {
            return SUCCESS; // Directory already exists
        } else {
            return ERROR_GENERAL_FAILURE; // Path exists but is not a directory
        }
    }

    // Create directory
    if (mkdir(path, 0755) != 0) {
        log_error("Failed to create directory: %s", path);
        return ERROR_GENERAL_FAILURE;
    }

    log_debug("Created directory: %s", path);
    return SUCCESS;
}

// Backup state data
int file_backup_state(const uint8_t* state_data, size_t state_size, const char* backup_id) {
    if (!state_data || !backup_id || state_size == 0) {
        return ERROR_NULL_POINTER;
    }

    char backup_filename[512];
    snprintf(backup_filename, sizeof(backup_filename), 
             "%s/state_%s_%llu.dat", 
             g_file_state.backup_directory, backup_id, (unsigned long long)time(NULL));

    log_info("Creating state backup: %s (%zu bytes)", backup_filename, state_size);

    return file_write(backup_filename, state_data, state_size);
}

// Restore state data
int file_restore_state(const char* backup_id, uint8_t* state_data, size_t* buffer_size) {
    if (!backup_id || !state_data || !buffer_size) {
        return ERROR_NULL_POINTER;
    }

    // Find the most recent backup with the given ID
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/state_%s_", g_file_state.backup_directory, backup_id);

    char backup_filename[512] = {0};
    time_t latest_time = 0;

#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_pattern[512];
    snprintf(search_pattern, sizeof(search_pattern), "%s*.dat", pattern);
    
    HANDLE hFind = FindFirstFile(search_pattern, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", g_file_state.backup_directory, find_data.cFileName);
            
            // Extract timestamp from filename
            char* time_str = strstr(find_data.cFileName, backup_id);
            if (time_str) {
                time_str += strlen(backup_id) + 1; // Skip backup_id and underscore
                time_t file_time = (time_t)atoll(time_str);
                if (file_time > latest_time) {
                    latest_time = file_time;
                    strcpy(backup_filename, full_path);
                }
            }
        } while (FindNextFile(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(g_file_state.backup_directory);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, pattern + strlen(g_file_state.backup_directory) + 1, 
                       strlen(pattern) - strlen(g_file_state.backup_directory) - 1) == 0) {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", g_file_state.backup_directory, entry->d_name);
                
                // Extract timestamp from filename
                char* time_str = strstr(entry->d_name, backup_id);
                if (time_str) {
                    time_str += strlen(backup_id) + 1;
                    time_t file_time = (time_t)atoll(time_str);
                    if (file_time > latest_time) {
                        latest_time = file_time;
                        strcpy(backup_filename, full_path);
                    }
                }
            }
        }
        closedir(dir);
    }
#endif

    if (strlen(backup_filename) == 0) {
        log_error("No backup found for ID: %s", backup_id);
        return ERROR_STORAGE_NOT_FOUND;
    }

    log_info("Restoring state from backup: %s", backup_filename);
    return file_read(backup_filename, state_data, buffer_size);
}

// Get file size
int file_get_size(const char* filename, size_t* file_size) {
    if (!filename || !file_size) {
        return ERROR_NULL_POINTER;
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        return ERROR_FILE_NOT_FOUND;
    }

    *file_size = (size_t)st.st_size;
    return SUCCESS;
}

// Get file modification time
int file_get_modification_time(const char* filename, uint64_t* mod_time) {
    if (!filename || !mod_time) {
        return ERROR_NULL_POINTER;
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        return ERROR_FILE_NOT_FOUND;
    }

    *mod_time = (uint64_t)st.st_mtime;
    return SUCCESS;
}

// New enclave_result_t-based file operations for integration tests

enclave_result_t write_file_data(const char* filename, const uint8_t* data, size_t data_size) {
    if (!filename || !data || data_size == 0) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    int result = file_write(filename, data, data_size);
    switch (result) {
        case SUCCESS:
            return ENCLAVE_SUCCESS;
        case ERROR_NULL_POINTER:
            return ENCLAVE_ERROR_INVALID_PARAMETER;        case ERROR_FILE_WRITE_FAILED:
            return ENCLAVE_ERROR_FILE_WRITE_FAILED;
        default:
            return ENCLAVE_ERROR_GENERAL;
    }
}

enclave_result_t read_file_data(const char* filename, uint8_t** data, size_t* data_size) {
    if (!filename || !data || !data_size) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Get file size first
    size_t file_size;
    int result = file_get_size(filename, &file_size);
    if (result != SUCCESS) {
        return ENCLAVE_ERROR_FILE_NOT_FOUND;
    }

    // Allocate memory for file data
    *data = (uint8_t*)malloc(file_size);
    if (!*data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    // Read file content
    size_t buffer_size = file_size;
    result = file_read(filename, *data, &buffer_size);
    if (result != SUCCESS) {
        free(*data);
        *data = NULL;
        switch (result) {
            case ERROR_FILE_NOT_FOUND:
                return ENCLAVE_ERROR_FILE_NOT_FOUND;            case ERROR_FILE_READ_FAILED:
                return ENCLAVE_ERROR_FILE_READ_FAILED;
            default:
                return ENCLAVE_ERROR_GENERAL;
        }
    }

    *data_size = buffer_size;
    return ENCLAVE_SUCCESS;
}

enclave_result_t delete_file(const char* filename) {
    if (!filename) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    int result = file_delete(filename);
    switch (result) {
        case SUCCESS:
            return ENCLAVE_SUCCESS;
        case ERROR_NULL_POINTER:
            return ENCLAVE_ERROR_INVALID_PARAMETER;
        case ERROR_FILE_NOT_FOUND:
            return ENCLAVE_ERROR_FILE_NOT_FOUND;        default:
            return ENCLAVE_ERROR_GENERAL;
    }
}

bool file_exists(const char* filename) {
    if (!filename) {
        return false;
    }

    int exists = 0;
    int result = file_exists_internal(filename, &exists);
    return (result == SUCCESS && exists == 1);
}

enclave_result_t create_file_backup(const char* source_filename, const char* backup_filename) {
    if (!source_filename || !backup_filename) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Read source file
    uint8_t* data;
    size_t data_size;
    enclave_result_t result = read_file_data(source_filename, &data, &data_size);
    if (result != ENCLAVE_SUCCESS) {
        return result;
    }

    // Write to backup file
    result = write_file_data(backup_filename, data, data_size);
    free(data);

    return result;
}

enclave_result_t write_sealed_data(const char* filename, const sealed_data_t* sealed_data, const uint8_t* data) {
    if (!filename || !sealed_data || !data) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    int result = file_write_sealed_data(filename, sealed_data);
    switch (result) {
        case SUCCESS:
            return ENCLAVE_SUCCESS;
        case ERROR_NULL_POINTER:
            return ENCLAVE_ERROR_INVALID_PARAMETER;
        case ERROR_FILE_WRITE_FAILED:
            return ENCLAVE_ERROR_FILE_WRITE_FAILED;        default:
            return ENCLAVE_ERROR_GENERAL;
    }
}

enclave_result_t read_sealed_data(const char* filename, sealed_data_t* sealed_data, uint8_t** data) {
    if (!filename || !sealed_data || !data) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    int result = file_read_sealed_data(filename, sealed_data);
    if (result != SUCCESS) {
        switch (result) {
            case ERROR_NULL_POINTER:
                return ENCLAVE_ERROR_INVALID_PARAMETER;
            case ERROR_FILE_NOT_FOUND:
                return ENCLAVE_ERROR_FILE_NOT_FOUND;            case ERROR_FILE_READ_FAILED:
                return ENCLAVE_ERROR_FILE_READ_FAILED;
            default:
                return ENCLAVE_ERROR_GENERAL;
        }
    }

    // Allocate memory for the unsealed data
    *data = (uint8_t*)malloc(sealed_data->data_size);
    if (!*data) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    // Copy the sealed data to the allocated buffer
    memcpy(*data, sealed_data->sealed_data, sealed_data->data_size);
    return ENCLAVE_SUCCESS;
}
