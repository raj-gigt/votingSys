#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "host_interface.h"
#include "shared_types.h"
#include "error_codes.h"
#include "constants.h"

// Simple integration test for the complete voting flow
int main(void) {
    printf("=== Integration Test: Complete Voting Flow ===\n");

    // Initialize host context
    host_context_t context;
    memset(&context, 0, sizeof(context));
    
    // Set default configuration
    context.config.port = 8080;
    context.config.log_level = 2; // INFO
    context.config.max_connections = 10;
    context.config.simulation_mode = true;
    strcpy(context.config.log_file, "logs/integration_test.log");

    printf("1. Initializing host interface...\n");
    enclave_result_t result = host_initialize(&context);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Host initialization failed: %s\n", get_error_description(result));
        return 1;
    }
    printf("   ✓ Host initialized successfully\n");

    // Test enclave info
    printf("2. Getting enclave information...\n");
    enclave_info_t info;
    result = host_get_enclave_info(&info);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Failed to get enclave info: %s\n", get_error_description(result));
        goto cleanup;
    }
    printf("   ✓ Enclave info: version=%s, votes=%u\n", info.version, info.total_votes);

    // Test keypair generation
    printf("3. Generating cryptographic keypair...\n");
    crypto_key_t public_key, private_key;
    result = host_generate_keypair(&public_key, &private_key);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Keypair generation failed: %s\n", get_error_description(result));
        goto cleanup;
    }
    printf("   ✓ Keypair generated (public: %zu bytes, private: %zu bytes)\n", 
           public_key.size, private_key.size);

    // Test vote processing
    printf("4. Processing test votes...\n");
    
    // Create and process multiple test votes
    for (int i = 0; i < 5; i++) {
        vote_t vote;
        memset(&vote, 0, sizeof(vote));
        
        // Create vote ID
        snprintf((char*)vote.vote_id, VOTE_ID_SIZE, "vote_%03d", i);
        vote.candidate_id = i % 3; // Candidates 0, 1, 2
        vote.timestamp = (uint64_t)time(NULL) + i;
        
        // Create dummy signature
        vote.signature.size = SIGNATURE_SIZE;
        for (size_t j = 0; j < SIGNATURE_SIZE; j++) {
            vote.signature.data[j] = (uint8_t)(0x42 + i + j);
        }
        
        vote_receipt_t receipt;
        result = host_process_vote(&vote, &receipt);
        if (result != ENCLAVE_SUCCESS) {
            printf("   ✗ Vote %d processing failed: %s\n", i, get_error_description(result));
            goto cleanup;
        }
        
        printf("   ✓ Vote %d processed (candidate: %u, status: %u)\n", 
               i, vote.candidate_id, receipt.status);
    }

    // Test vote aggregation
    printf("5. Getting vote aggregation...\n");
    vote_aggregation_t aggregation;
    result = host_aggregate_votes(&aggregation);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Vote aggregation failed: %s\n", get_error_description(result));
        goto cleanup;
    }
    
    printf("   ✓ Aggregation results:\n");
    printf("     Total votes: %u\n", aggregation.total_votes);
    for (int i = 0; i < 3; i++) {
        printf("     Candidate %d: %u votes\n", i, aggregation.candidate_votes[i]);
    }

    // Test data signing and verification
    printf("6. Testing data signing and verification...\n");
    const char* test_message = "Integration test message";
    crypto_signature_t signature;
    
    result = host_sign_data((const uint8_t*)test_message, strlen(test_message), 
                           &private_key, &signature);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Data signing failed: %s\n", get_error_description(result));
        goto cleanup;
    }
    
    result = host_verify_signature((const uint8_t*)test_message, strlen(test_message),
                                  &signature, &public_key);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Signature verification failed: %s\n", get_error_description(result));
        goto cleanup;
    }
    printf("   ✓ Data signing and verification successful\n");

    // Test sealed storage
    printf("7. Testing sealed storage...\n");
    const char* sensitive_data = "Sensitive voting data for integration test";
    uint8_t* sealed_data = NULL;
    size_t sealed_len = 0;
    
    result = host_seal_data((const uint8_t*)sensitive_data, strlen(sensitive_data), 
                           &sealed_data, &sealed_len);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Data sealing failed: %s\n", get_error_description(result));
        goto cleanup;
    }
    
    uint8_t* unsealed_data = NULL;
    size_t unsealed_len = 0;
    result = host_unseal_data(sealed_data, sealed_len, &unsealed_data, &unsealed_len);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Data unsealing failed: %s\n", get_error_description(result));
        if (sealed_data) free(sealed_data);
        goto cleanup;
    }
    
    if (strlen(sensitive_data) == unsealed_len && 
        memcmp(sensitive_data, unsealed_data, unsealed_len) == 0) {
        printf("   ✓ Sealed storage test successful\n");
    } else {
        printf("   ✗ Sealed storage data integrity check failed\n");
    }
    
    if (sealed_data) free(sealed_data);
    if (unsealed_data) free(unsealed_data);

    // Final enclave info check
    printf("8. Final state verification...\n");
    result = host_get_enclave_info(&info);
    if (result != ENCLAVE_SUCCESS) {
        printf("   ✗ Failed to get final enclave info: %s\n", get_error_description(result));
        goto cleanup;
    }
    
    printf("   ✓ Final state: total_votes=%u, valid_votes=%u, invalid_votes=%u\n",
           info.total_votes, info.valid_votes, info.invalid_votes);

    printf("\n=== Integration Test PASSED ===\n");
    printf("All components are working together correctly!\n");

cleanup:
    // Cleanup host interface
    host_cleanup(&context);
    
    return (result == ENCLAVE_SUCCESS) ? 0 : 1;
}
