#include "test_framework.h"
#include "shared_types.h"
#include "error_codes.h"
#include "enclave_operations.h"
#include <string.h>
#include <time.h>

// Mock vote creation helper
static void create_test_vote(vote_t* vote, uint32_t candidate_id, const char* vote_id_str) {
    memset(vote, 0, sizeof(vote_t));
    vote->candidate_id = candidate_id;
    vote->timestamp = (uint64_t)time(NULL);
    
    // Convert vote ID string to bytes
    size_t id_len = strlen(vote_id_str);
    for (size_t i = 0; i < VOTE_ID_SIZE && i < id_len; i++) {
        vote->vote_id[i] = (uint8_t)vote_id_str[i];
    }
    
    // Create dummy signature
    vote->signature.size = SIGNATURE_SIZE;
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        vote->signature.data[i] = (uint8_t)(0x42 + (i % 16));
    }
}

void test_vote_processing(void) {
    // Test enclave initialization
    enclave_result_t result = initialize_enclave_state();
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Enclave initialization should succeed");

    // Test vote validation
    vote_t valid_vote;
    create_test_vote(&valid_vote, 1, "test_vote_001");
    
    result = validate_vote(&valid_vote);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Valid vote should pass validation");

    // Test invalid candidate ID
    vote_t invalid_vote;
    create_test_vote(&invalid_vote, MAX_CANDIDATES + 1, "test_vote_002");
    
    result = validate_vote(&invalid_vote);
    TEST_ASSERT_EQUAL(ENCLAVE_ERROR_INVALID_CANDIDATE, result, "Invalid candidate should fail validation");

    // Test vote processing
    vote_receipt_t receipt;
    result = process_vote(&valid_vote, &receipt);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Valid vote processing should succeed");
    TEST_ASSERT_EQUAL(VOTE_STATUS_ACCEPTED, receipt.status, "Vote should be accepted");

    // Test vote aggregation
    vote_aggregation_t aggregation;
    result = aggregate_votes(&aggregation);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Vote aggregation should succeed");
    TEST_ASSERT_EQUAL(1, aggregation.total_votes, "Should have 1 total vote");
    TEST_ASSERT_EQUAL(1, aggregation.candidate_votes[1], "Candidate 1 should have 1 vote");

    // Test multiple votes
    vote_t vote2, vote3;
    create_test_vote(&vote2, 1, "test_vote_003");
    create_test_vote(&vote3, 2, "test_vote_004");

    result = process_vote(&vote2, &receipt);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Second vote processing should succeed");

    result = process_vote(&vote3, &receipt);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Third vote processing should succeed");

    result = aggregate_votes(&aggregation);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Vote aggregation after multiple votes should succeed");
    TEST_ASSERT_EQUAL(3, aggregation.total_votes, "Should have 3 total votes");
    TEST_ASSERT_EQUAL(2, aggregation.candidate_votes[1], "Candidate 1 should have 2 votes");
    TEST_ASSERT_EQUAL(1, aggregation.candidate_votes[2], "Candidate 2 should have 1 vote");

    // Test enclave info
    enclave_info_t info;
    result = get_enclave_info(&info);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Getting enclave info should succeed");
    TEST_ASSERT_EQUAL(3, info.total_votes, "Info should show 3 total votes");
    TEST_ASSERT_EQUAL(3, info.valid_votes, "Info should show 3 valid votes");
    TEST_ASSERT_EQUAL(0, info.invalid_votes, "Info should show 0 invalid votes");

    // Cleanup
    result = cleanup_enclave_state();
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Enclave cleanup should succeed");
}
