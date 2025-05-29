#include "test_framework.h"
#include "shared_types.h"
#include "error_codes.h"

// Test function declarations
void test_vote_processing(void);
void test_crypto_operations(void);
void test_network_interface(void);
void test_file_operations(void);

int main(void) {
    printf("=== Enclave Collector Unit Tests ===\n");
    printf("Running in simulation mode\n");

    // Run all test suites
    RUN_TEST(test_vote_processing);
    RUN_TEST(test_crypto_operations);
    RUN_TEST(test_network_interface);
    RUN_TEST(test_file_operations);

    // Print summary and return result
    print_test_summary();
    return get_test_result();
}
