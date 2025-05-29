#include "test_framework.h"
#include "file_operations.h"
#include "shared_types.h"
#include "error_codes.h"
#include <string.h>
#include <stdio.h>

void test_file_operations(void) {
    const char* test_filename = "test_data.tmp";
    const char* test_data = "This is test data for file operations";
    size_t test_data_len = strlen(test_data);

    // Test file writing
    enclave_result_t result = write_file_data(test_filename, (const uint8_t*)test_data, test_data_len);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "File writing should succeed");

    // Test file existence check
    bool exists = file_exists(test_filename);
    TEST_ASSERT(exists, "File should exist after writing");

    // Test file reading
    uint8_t* read_data = NULL;
    size_t read_data_len = 0;
    result = read_file_data(test_filename, &read_data, &read_data_len);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "File reading should succeed");
    TEST_ASSERT_NOT_NULL(read_data, "Read data should not be null");
    TEST_ASSERT_EQUAL(test_data_len, read_data_len, "Read data length should match written length");
    TEST_ASSERT(memcmp(test_data, read_data, test_data_len) == 0, 
               "Read data should match written data");

    // Test file backup
    const char* backup_filename = "test_data_backup.tmp";
    result = create_file_backup(test_filename, backup_filename);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "File backup creation should succeed");

    bool backup_exists = file_exists(backup_filename);
    TEST_ASSERT(backup_exists, "Backup file should exist");

    // Test backup data integrity
    uint8_t* backup_data = NULL;
    size_t backup_data_len = 0;
    result = read_file_data(backup_filename, &backup_data, &backup_data_len);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Backup file reading should succeed");
    TEST_ASSERT_EQUAL(test_data_len, backup_data_len, "Backup data length should match original");
    TEST_ASSERT(memcmp(test_data, backup_data, test_data_len) == 0, 
               "Backup data should match original data");

    // Test sealed data writing
    const char* sealed_filename = "sealed_data.tmp";
    sealed_data_t sealed_data;
    sealed_data.magic = SEALED_DATA_MAGIC;
    sealed_data.data_size = test_data_len;
    sealed_data.sealed_size = test_data_len + 64; // Mock sealed size
    
    result = write_sealed_data(sealed_filename, &sealed_data, (const uint8_t*)test_data);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Sealed data writing should succeed");

    // Test sealed data reading
    sealed_data_t read_sealed_data;
    uint8_t* read_sealed_content = NULL;
    result = read_sealed_data(sealed_filename, &read_sealed_data, &read_sealed_content);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Sealed data reading should succeed");
    TEST_ASSERT_EQUAL(SEALED_DATA_MAGIC, read_sealed_data.magic, "Sealed data magic should match");
    TEST_ASSERT_EQUAL(test_data_len, read_sealed_data.data_size, "Sealed data size should match");
    TEST_ASSERT_NOT_NULL(read_sealed_content, "Sealed content should not be null");

    // Test file deletion
    result = delete_file(test_filename);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "File deletion should succeed");

    exists = file_exists(test_filename);
    TEST_ASSERT(!exists, "File should not exist after deletion");

    // Test reading non-existent file
    uint8_t* nonexistent_data = NULL;
    size_t nonexistent_len = 0;
    result = read_file_data("nonexistent_file.tmp", &nonexistent_data, &nonexistent_len);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_FILE_NOT_FOUND, result, "Reading non-existent file should fail");
    TEST_ASSERT_NULL(nonexistent_data, "Data from non-existent file should be null");

    // Test invalid parameters
    result = write_file_data(NULL, (const uint8_t*)test_data, test_data_len);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_INVALID_PARAMETER, result, "Writing with null filename should fail");

    result = write_file_data(test_filename, NULL, test_data_len);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_INVALID_PARAMETER, result, "Writing null data should fail");

    // Cleanup
    if (read_data) free(read_data);
    if (backup_data) free(backup_data);
    if (read_sealed_content) free(read_sealed_content);
    
    // Clean up test files
    delete_file(backup_filename);
    delete_file(sealed_filename);
}
