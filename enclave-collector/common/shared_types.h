#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum sizes and limits
#define MAX_VOTE_SIZE 1024
#define MAX_VOTER_ID_SIZE 64
#define MAX_CANDIDATE_COUNT 256
#define MAX_BALLOT_SIZE 2048
#define MAX_SIGNATURE_SIZE 512
#define MAX_PUBLIC_KEY_SIZE 512
#define MAX_PRIVATE_KEY_SIZE 512
#define MAX_ENCRYPTED_DATA_SIZE 2048
#define MAX_SEALED_DATA_SIZE 4096

// Constants for sizes
#define VOTE_ID_SIZE 32
#define HASH_SIZE 32
#define SIGNATURE_SIZE 64
#define MAX_CANDIDATES 256
#define CRYPTO_KEY_SIZE 32

// Result/Error type for enclave operations
typedef enum {
    ENCLAVE_SUCCESS = 0,
    
    // General errors (1-20)
    ENCLAVE_ERROR_GENERAL = 1,
    ENCLAVE_ERROR_INVALID_PARAMETER = 2,
    ENCLAVE_ERROR_NULL_POINTER = 3,
    ENCLAVE_ERROR_BUFFER_TOO_SMALL = 4,
    ENCLAVE_ERROR_OUT_OF_MEMORY = 5,
    ENCLAVE_ERROR_NOT_INITIALIZED = 6,
    ENCLAVE_ERROR_ALREADY_INITIALIZED = 7,
    ENCLAVE_ERROR_OPERATION_FAILED = 8,
    ENCLAVE_ERROR_NOT_IMPLEMENTED = 9,
    ENCLAVE_ERROR_MEMORY_ALLOCATION = 10,
    
    // Initialization errors (21-30)
    ENCLAVE_ERROR_INITIALIZATION_FAILED = 21,
    ENCLAVE_ERROR_CLEANUP_FAILED = 22,
    ENCLAVE_ERROR_ALREADY_RUNNING = 23,
    
    // Cryptographic errors (31-50)
    ENCLAVE_ERROR_CRYPTO_FAILED = 31,
    ENCLAVE_ERROR_INVALID_SIGNATURE = 32,
    ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED = 33,
    ENCLAVE_ERROR_KEY_GENERATION_FAILED = 34,
    ENCLAVE_ERROR_INVALID_KEY_SIZE = 35,
    ENCLAVE_ERROR_ENCRYPTION_FAILED = 36,
    ENCLAVE_ERROR_DECRYPTION_FAILED = 37,
    ENCLAVE_ERROR_HASH_FAILED = 38,
    
    // Vote processing errors (51-70)
    ENCLAVE_ERROR_INVALID_VOTE_ID = 51,
    ENCLAVE_ERROR_INVALID_CANDIDATE = 52,
    ENCLAVE_ERROR_INVALID_TIMESTAMP = 53,
    ENCLAVE_ERROR_DUPLICATE_VOTE = 54,
    ENCLAVE_ERROR_VOTE_BUFFER_FULL = 55,
    ENCLAVE_ERROR_INVALID_VOTE = 56,
    ENCLAVE_ERROR_VOTE_VERIFICATION_FAILED = 57,
    
    // Sealed storage errors (71-90)
    ENCLAVE_ERROR_SEALING_FAILED = 71,
    ENCLAVE_ERROR_UNSEALING_FAILED = 72,
    ENCLAVE_ERROR_INVALID_SEALED_DATA = 73,
    ENCLAVE_ERROR_SEALED_DATA_INTEGRITY = 74,
    ENCLAVE_ERROR_SEALED_DATA_TOO_LARGE = 75,
    ENCLAVE_ERROR_NO_SEALED_DATA = 76,
      // Network errors (91-110)
    ENCLAVE_ERROR_NETWORK_INIT = 91,
    ENCLAVE_ERROR_NETWORK_SOCKET = 92,
    ENCLAVE_ERROR_NETWORK_BIND = 93,
    ENCLAVE_ERROR_NETWORK_LISTEN = 94,
    ENCLAVE_ERROR_NETWORK_RECEIVE = 95,
    ENCLAVE_ERROR_NETWORK_SEND = 96,
    ENCLAVE_ERROR_INVALID_HTTP_REQUEST = 97,
    ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED = 98,
    
    // Thread errors (111-120)
    ENCLAVE_ERROR_THREAD_CREATE = 111,
    ENCLAVE_ERROR_THREAD_JOIN = 112,
      // File I/O errors (121-130)
    ENCLAVE_ERROR_FILE_NOT_FOUND = 121,
    ENCLAVE_ERROR_FILE_READ_FAILED = 122,
    ENCLAVE_ERROR_FILE_WRITE_FAILED = 123,
    ENCLAVE_ERROR_FILE_PERMISSION = 124,
    
    // API errors (131-140)
    ENCLAVE_ERROR_API_NOT_INITIALIZED = 131,
    ENCLAVE_ERROR_KEY_NOT_FOUND = 132,
    ENCLAVE_ERROR_INVALID_STATE = 133
} enclave_result_t;

// Vote status enumeration
typedef enum {
    VOTE_STATUS_PENDING = 0,
    VOTE_STATUS_ACCEPTED = 1,
    VOTE_STATUS_REJECTED = 2,
    VOTE_STATUS_INVALID = 3
} vote_status_t;

// Signature structure
typedef struct {
    uint8_t data[SIGNATURE_SIZE];
    size_t size;
} signature_t;

// Cryptographic key structure
typedef struct {
    uint8_t data[CRYPTO_KEY_SIZE];
    size_t size;
} crypto_key_t;

// Cryptographic signature structure
typedef struct {
    uint8_t data[SIGNATURE_SIZE];
    size_t size;
} crypto_signature_t;

// Vote structure
typedef struct {
    uint8_t vote_id[VOTE_ID_SIZE];
    uint32_t candidate_id;
    uint64_t timestamp;
    signature_t signature;
    uint8_t encrypted_vote[MAX_VOTE_SIZE];
    size_t encrypted_vote_size;
} vote_t;

// Vote receipt structure
typedef struct {
    uint8_t vote_id[VOTE_ID_SIZE];
    uint64_t timestamp;
    vote_status_t status;
    uint8_t receipt_hash[HASH_SIZE];
} vote_receipt_t;

// Vote aggregation structure
typedef struct {
    uint32_t candidate_votes[MAX_CANDIDATES];
    uint32_t total_votes;
} vote_aggregation_t;

// Collector state structure
typedef struct {
    uint32_t total_votes;
    uint32_t valid_votes;
    uint32_t invalid_votes;
    bool is_sealed;
    vote_aggregation_t aggregation;
} collector_state_t;

// Enclave information structure
typedef struct {
    char version[16];
    uint32_t total_votes;
    uint32_t valid_votes;
    uint32_t invalid_votes;
    bool is_sealed;
} enclave_info_t;

// Cryptographic key pair
typedef struct {
    uint8_t public_key[MAX_PUBLIC_KEY_SIZE];
    size_t public_key_size;
    uint8_t private_key[MAX_PRIVATE_KEY_SIZE];
    size_t private_key_size;
} key_pair_t;

// Encrypted data container
typedef struct {
    uint8_t data[MAX_ENCRYPTED_DATA_SIZE];
    size_t data_size;
    uint8_t iv[16]; // Initialization vector
    uint8_t tag[16]; // Authentication tag for AEAD
} encrypted_data_t;

// Sealed data container (for enclave persistent storage)
typedef struct {
    uint32_t magic;                                // Magic number for validation
    size_t data_size;                             // Size of original data
    size_t sealed_size;                           // Size of sealed data
    uint8_t sealed_data[MAX_SEALED_DATA_SIZE];    // Sealed data
    size_t sealed_data_size;                      // Actual sealed data size
} sealed_data_t;

// Vote aggregation result
typedef struct {
    uint32_t candidate_id;
    uint64_t vote_count;
} vote_result_t;

// Aggregation summary
typedef struct {
    vote_result_t results[MAX_CANDIDATE_COUNT];
    size_t candidate_count;
    uint64_t total_votes_counted;
    uint8_t aggregation_proof[MAX_SIGNATURE_SIZE];
    size_t proof_size;
} aggregation_summary_t;

// Network message structure
typedef struct {
    uint32_t message_type;
    uint32_t message_id;
    size_t payload_size;
    uint8_t payload[];
} network_message_t;

// File operation structure
typedef struct {
    char filename[256];
    uint8_t* data;
    size_t data_size;
    uint32_t operation_type; // READ, WRITE, APPEND
} file_operation_t;

// System information structure
typedef struct {
    char hostname[256];
    uint64_t total_memory;
    uint64_t available_memory;
    uint32_t cpu_count;
    uint64_t uptime;
} system_info_t;

// Vote processing statistics
typedef struct {
    uint32_t processed_votes;
    uint32_t buffer_capacity;
    uint32_t buffer_used;
    uint32_t buffer_available;
} vote_processing_stats_t;

// Sealed storage information
typedef struct {
    bool is_initialized;
    size_t current_size;
    size_t max_size;
    size_t available_size;
} sealed_storage_info_t;

// Election parameters structure (from external API)
typedef struct {
    uint32_t num_candidates;
    uint32_t max_votes;
    uint64_t start_time;
    uint64_t end_time;
    uint8_t public_key[MAX_PUBLIC_KEY_SIZE];
    size_t public_key_size;
    char election_name[MAX_ELECTION_NAME_SIZE];
} election_params_t;

// Auxiliary values structure (from external API)
typedef struct {
    char voter_id[128];
    uint8_t aux_value[1024];
    size_t aux_value_size;
    uint64_t timestamp;
} auxiliary_values_t;

// Final election results structure
typedef struct {
    char election_id[MAX_ELECTION_ID_SIZE];
    uint32_t total_votes;
    uint32_t candidate_count;
    uint32_t candidate_votes[MAX_CANDIDATES];
    uint64_t timestamp;
    uint8_t result_proof[MAX_SIGNATURE_SIZE];
    size_t proof_size;
} final_results_t;

#ifdef __cplusplus
}
#endif

#endif // SHARED_TYPES_H
