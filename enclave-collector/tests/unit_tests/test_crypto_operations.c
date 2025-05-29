#include "test_framework.h"
#include "shared_types.h"
#include "error_codes.h"
#include "enclave_operations.h"
#include <string.h>

void test_crypto_operations(void) {
    // Test keypair generation
    crypto_key_t public_key, private_key;
    enclave_result_t result = generate_keypair(&public_key, &private_key);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Keypair generation should succeed");
    TEST_ASSERT_EQUAL(CRYPTO_KEY_SIZE, public_key.size, "Public key should have correct size");
    TEST_ASSERT_EQUAL(CRYPTO_KEY_SIZE, private_key.size, "Private key should have correct size");

    // Test data signing
    const char* test_data = "Hello, secure voting system!";
    size_t data_len = strlen(test_data);
    crypto_signature_t signature;

    result = sign_data((const uint8_t*)test_data, data_len, &private_key, &signature);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Data signing should succeed");
    TEST_ASSERT_EQUAL(SIGNATURE_SIZE, signature.size, "Signature should have correct size");

    // Test signature verification
    result = verify_signature((const uint8_t*)test_data, data_len, &signature, &public_key);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Signature verification should succeed");

    // Test signature verification with wrong data
    const char* wrong_data = "Wrong data";
    result = verify_signature((const uint8_t*)wrong_data, strlen(wrong_data), &signature, &public_key);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED, result, 
                     "Signature verification with wrong data should fail");

    // Test with different keypair
    crypto_key_t wrong_public_key, wrong_private_key;
    result = generate_keypair(&wrong_public_key, &wrong_private_key);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Second keypair generation should succeed");

    result = verify_signature((const uint8_t*)test_data, data_len, &signature, &wrong_public_key);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED, result, 
                     "Signature verification with wrong key should fail");

    // Test invalid parameters
    result = sign_data(NULL, data_len, &private_key, &signature);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_INVALID_PARAMETER, result, "Signing null data should fail");

    result = verify_signature((const uint8_t*)test_data, data_len, NULL, &public_key);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_INVALID_PARAMETER, result, "Verifying null signature should fail");

    // Test sealed storage
    const char* sensitive_data = "This is sensitive voting data";
    size_t sensitive_len = strlen(sensitive_data);
    uint8_t* sealed_data = NULL;
    size_t sealed_len = 0;

    result = seal_data((const uint8_t*)sensitive_data, sensitive_len, &sealed_data, &sealed_len);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Data sealing should succeed");
    TEST_ASSERT_NOT_NULL(sealed_data, "Sealed data should not be null");
    TEST_ASSERT(sealed_len > sensitive_len, "Sealed data should be larger than original");

    // Test data unsealing
    uint8_t* unsealed_data = NULL;
    size_t unsealed_len = 0;

    result = unseal_data(sealed_data, sealed_len, &unsealed_data, &unsealed_len);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Data unsealing should succeed");
    TEST_ASSERT_NOT_NULL(unsealed_data, "Unsealed data should not be null");
    TEST_ASSERT_EQUAL(sensitive_len, unsealed_len, "Unsealed data length should match original");
    TEST_ASSERT(memcmp(sensitive_data, unsealed_data, sensitive_len) == 0, 
               "Unsealed data should match original data");

    // Cleanup
    if (sealed_data) free(sealed_data);
    if (unsealed_data) free(unsealed_data);
}
