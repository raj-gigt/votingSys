#ifndef CONSTANTS_H
#define CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Cryptographic constants
#define AES_KEY_SIZE 32         // 256-bit AES key
#define AES_IV_SIZE 16          // 128-bit IV
#define AES_TAG_SIZE 16         // 128-bit authentication tag
#define RSA_KEY_SIZE 2048       // RSA key size in bits
#define SHA256_HASH_SIZE 32     // SHA-256 hash size
#define ECDSA_KEY_SIZE 32       // ECDSA P-256 key size

// Network constants
#define DEFAULT_PORT 8080
#define MAX_CONNECTIONS 100
#define NETWORK_TIMEOUT_MS 30000
#define MAX_MESSAGE_SIZE 65536
#define KEEPALIVE_INTERVAL_MS 10000

// Vote processing constants
#define MIN_VOTE_AGE_MS 1000           // Minimum time between votes
#define MAX_VOTE_AGE_MS 86400000       // Maximum vote age (24 hours)
#define VOTE_BATCH_SIZE 100            // Number of votes to process in batch
#define MAX_VOTES_PER_BATCH 100        // Maximum votes per processing batch
#define MAX_RETRIES 3                  // Maximum retry attempts

// Election constants (using shared_types.h definitions to avoid conflicts)
// Note: Election size constants are defined in shared_types.h
#define ELECTION_TIMEOUT_HOURS 168     // 7 days maximum election duration

// Storage constants
#define SEALED_DATA_HEADER_SIZE 64
#define MAX_FILENAME_LENGTH 256
#define STORAGE_BLOCK_SIZE 4096
#define BACKUP_RETENTION_DAYS 30

// Logging constants
#define MAX_LOG_MESSAGE_SIZE 512
#define LOG_BUFFER_SIZE 8192
#define MAX_LOG_FILE_SIZE 10485760     // 10MB
#define LOG_ROTATION_COUNT 5

// Performance constants
#define THREAD_POOL_SIZE 4
#define QUEUE_MAX_SIZE 1000
#define MEMORY_POOL_SIZE 1048576       // 1MB
#define CACHE_SIZE 512

// Protocol version and magic numbers
#define PROTOCOL_VERSION 1
#define VOTE_MAGIC_NUMBER 0x564F5445   // "VOTE" in hex
#define COLLECTOR_MAGIC_NUMBER 0x434F4C4C // "COLL" in hex
#define ENCLAVE_MAGIC_NUMBER 0x454E434C   // "ENCL" in hex
#define SEALED_DATA_MAGIC 0x5345414C      // "SEAL" in hex

// Time constants
#define TIMESTAMP_TOLERANCE_MS 5000    // 5 seconds tolerance for timestamps
#define SESSION_TIMEOUT_MS 1800000     // 30 minutes
#define HEARTBEAT_INTERVAL_MS 60000    // 1 minute

// Simulation mode constants
#define SIMULATION_ENCLAVE_ID "simulation-collector-v1"
#define SIMULATION_KEY_DERIVE_ROUNDS 10000
#define SIMULATION_SEAL_KEY_SIZE 32

// Message types
#define MSG_TYPE_VOTE_SUBMISSION 1
#define MSG_TYPE_VOTE_RESPONSE 2
#define MSG_TYPE_AGGREGATION_REQUEST 3
#define MSG_TYPE_AGGREGATION_RESPONSE 4
#define MSG_TYPE_STATUS_REQUEST 5
#define MSG_TYPE_STATUS_RESPONSE 6
#define MSG_TYPE_ATTESTATION_REQUEST 7
#define MSG_TYPE_ATTESTATION_RESPONSE 8

// File operation types
#define FILE_OP_READ 1
#define FILE_OP_WRITE 2
#define FILE_OP_APPEND 3
#define FILE_OP_DELETE 4

// Security levels
#define SECURITY_LEVEL_NONE 0
#define SECURITY_LEVEL_BASIC 1
#define SECURITY_LEVEL_HIGH 2
#define SECURITY_LEVEL_MAXIMUM 3

#ifdef __cplusplus
}
#endif

#endif // CONSTANTS_H
