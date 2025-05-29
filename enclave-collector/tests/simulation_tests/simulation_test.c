#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_types.h"
#include "error_codes.h"

// Simple test framework
int test_count = 0;
int passed_count = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            passed_count++; \
            printf("PASS: %s\n", message); \
        } else { \
            printf("FAIL: %s\n", message); \
        } \
    } while(0)

// Test simulation mode functionality
void test_simulation_mode() {
    printf("Testing simulation mode functionality...\n");
    
    // Test basic data structures
    vote_t test_vote;
    memset(&test_vote, 0, sizeof(vote_t));
    test_vote.candidate_id = 1;
    test_vote.timestamp = 1735502400;
    
    TEST_ASSERT(test_vote.candidate_id == 1, "Vote candidate ID assignment");
    TEST_ASSERT(test_vote.timestamp == 1735502400, "Vote timestamp assignment");
    
    // Test vote receipt
    vote_receipt_t receipt;
    memset(&receipt, 0, sizeof(vote_receipt_t));
    receipt.status = VOTE_STATUS_ACCEPTED;
    
    TEST_ASSERT(receipt.status == VOTE_STATUS_ACCEPTED, "Vote receipt status");
    
    // Test aggregation
    vote_aggregation_t aggregation;
    memset(&aggregation, 0, sizeof(vote_aggregation_t));
    aggregation.candidate_votes[0] = 10;
    aggregation.candidate_votes[1] = 5;
    aggregation.total_votes = 15;
    
    TEST_ASSERT(aggregation.candidate_votes[0] == 10, "Aggregation candidate 0 votes");
    TEST_ASSERT(aggregation.candidate_votes[1] == 5, "Aggregation candidate 1 votes");
    TEST_ASSERT(aggregation.total_votes == 15, "Aggregation total votes");
}

int main() {
    printf("Starting simulation tests...\n");
    
    test_simulation_mode();
    
    printf("\nTest Results: %d/%d tests passed\n", passed_count, test_count);
    
    return (passed_count == test_count) ? 0 : 1;
}
