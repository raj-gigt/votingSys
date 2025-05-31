#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum sizes and limits (simplified)
#define MAX_AUXILIARY_VALUES 1000
#define MAX_API_RESPONSE_SIZE 4096
#define MAX_ELECTION_ID_SIZE 256
#define MAX_PUBLIC_KEY_SIZE 512
#define MAX_PRIVATE_KEY_SIZE 512
#define MAX_ENCRYPTED_DATA_SIZE 2048

// Constants for sizes
#define HASH_SIZE 32
#define SIGNATURE_SIZE 64
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
      // Auxiliary processing errors (51-70)
    ENCLAVE_ERROR_INVALID_AUXILIARY_VALUE = 51,
    ENCLAVE_ERROR_AUXILIARY_BUFFER_FULL = 52,
    ENCLAVE_ERROR_AUXILIARY_VERIFICATION_FAILED = 53,
    ENCLAVE_ERROR_AUXILIARY_ALREADY_PROCESSED = 54,
    ENCLAVE_ERROR_AUXILIARY_COMPUTATION_FAILED = 55,    ENCLAVE_ERROR_AUXILIARY_AGGREGATION_FAILED = 56,
    
    // Network errors (71-90)
    ENCLAVE_ERROR_NETWORK_INIT = 71,
    ENCLAVE_ERROR_NETWORK_SOCKET = 72,
    ENCLAVE_ERROR_NETWORK_BIND = 73,
    ENCLAVE_ERROR_NETWORK_LISTEN = 74,
    ENCLAVE_ERROR_NETWORK_RECEIVE = 75,
    ENCLAVE_ERROR_NETWORK_SEND = 76,
    ENCLAVE_ERROR_INVALID_HTTP_REQUEST = 77,
    ENCLAVE_ERROR_NETWORK_INITIALIZATION_FAILED = 78,
    ENCLAVE_ERROR_NETWORK_REQUEST_FAILED = 79,
    
    // Thread errors (81-90)
    ENCLAVE_ERROR_THREAD_CREATE = 81,
    ENCLAVE_ERROR_THREAD_JOIN = 82,
      // API errors (91-100)
    ENCLAVE_ERROR_API_NOT_INITIALIZED = 91,
    ENCLAVE_ERROR_KEY_NOT_FOUND = 92,
    ENCLAVE_ERROR_INVALID_STATE = 93,
    ENCLAVE_ERROR_API_RESPONSE_INVALID = 94,
    ENCLAVE_ERROR_API_COMMUNICATION = 95,
    ENCLAVE_ERROR_API_DATA_FORMAT = 96,
    
    // Additional cryptographic errors (101-120)
    ENCLAVE_ERROR_INVALID_PROOF = 101,
    ENCLAVE_ERROR_NO_DATA = 102,
    ENCLAVE_ERROR_BIGINT_ERROR = 103,
    ENCLAVE_ERROR_MATH_OPERATION_FAILED = 104,
    ENCLAVE_ERROR_CRYPTO_PROCESSOR_FAILED = 105,
    ENCLAVE_ERROR_NOT_SUPPORTED = 106
} enclave_result_t;

// Cryptographic signature structure
typedef struct {
    uint8_t data[SIGNATURE_SIZE];
    size_t size;
    uint8_t algorithm;  // Signature algorithm type
} crypto_signature_t;

// Auxiliary aggregation product structure
typedef struct {
    char product_hex[4096];     // Final auxiliary product (hex string)
    uint32_t total_values;      // Number of auxiliary values aggregated
    uint64_t computation_time;  // Time taken for computation (ms)
    uint8_t proof[256];         // Zero-knowledge proof
    size_t proof_size;          // Size of the proof
    uint64_t timestamp;         // When aggregation was completed
    bool is_valid;              // Whether the product is valid
} auxiliary_product_t;

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

// Auxiliary value structure for the mathematical protocol
struct auxiliary_value {
    char aux_hex[2048];     // aux_i = pk_A^sk_i (hex string)
    uint32_t user_id;
    uint64_t timestamp;
    uint8_t signature[64];  // Cryptographic signature for verification
};

// API auxiliary value structure (simplified for API communication)
struct api_auxiliary_value {
    char voter_id[128];
    char aux_value[1024];
    size_t aux_value_size;
    uint64_t timestamp;
};

// Cryptographic parameters structure
struct crypto_params {
    char N[2048];        // The modulus N = p*q (hex string)
    char H[2048];        // The hash function output in Z_N^2* (hex string)
    char skA[2048];      // Aggregator secret key (hex string)
    char election_id[128];
};

// Auxiliary state structure
struct auxiliary_state {
    bool initialized;
    uint32_t processed_count;
    uint64_t session_start;
    uint64_t last_aggregation;
    bool is_sealed;
};

// Forward declarations and missing types
typedef struct auxiliary_value auxiliary_value_t;
typedef struct api_auxiliary_value api_auxiliary_value_t;
typedef struct crypto_params crypto_params_t;
typedef struct auxiliary_state auxiliary_state_t;

// Auxiliary values structure (from external API) - simplified
typedef struct {
    uint32_t count;
    struct {
        char voter_id[128];
        uint8_t aux_value[1024];
        size_t aux_value_size;
        uint64_t timestamp;
    } values[MAX_AUXILIARY_VALUES];
} auxiliary_values_t;

// API response structure for client-server communication
typedef struct {
    int status_code;
    char message[MAX_API_RESPONSE_SIZE];
    size_t message_length;
    bool success;
} api_response_t;

// Collector state structure (simplified)
typedef struct {
    uint32_t auxiliary_count;
    bool initialized;
} collector_state_t;

// Enclave information structure (simplified)
typedef struct {
    char version[16];
    uint32_t auxiliary_count;
} enclave_info_t;

// Final aggregation result structure
typedef struct {
    char aux_hex[4096];     // Final auxiliary product (hex string from enclave)
    uint32_t total_users;
    uint64_t computation_time;
    uint8_t aggregation_proof[256]; // Zero-knowledge proof from enclave
} aggregation_result_t;

// Aggregation summary structure (same as aggregation result)
typedef struct {
    char aux_hex[4096];     // Final auxiliary product (hex string from enclave)
    uint32_t total_users;
    uint64_t computation_time;
    uint8_t aggregation_proof[256]; // Zero-knowledge proof from enclave
} aggregation_summary_t;

#ifdef __cplusplus
}
#endif

#endif // SHARED_TYPES_H
