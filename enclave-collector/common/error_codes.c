#include "error_codes.h"

const char* get_error_description(int error_code) {
    // First check if it's a new enclave_result_t error code
    if (error_code >= 0 && error_code <= 200) {
        return get_enclave_error_description((enclave_result_t)error_code);
    }
    
    // Handle old error codes
    switch (error_code) {
        case SUCCESS:
            return "Operation completed successfully";
        
        // General errors
        case ERROR_GENERAL_FAILURE:
            return "General failure occurred";
        case ERROR_INVALID_PARAMETER:
            return "Invalid parameter provided";
        case ERROR_NULL_POINTER:
            return "Null pointer encountered";
        case ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small for operation";
        case ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case ERROR_NOT_IMPLEMENTED:
            return "Feature not implemented";
        case ERROR_OPERATION_FAILED:
            return "Operation failed";
        
        // Initialization errors
        case ERROR_ENCLAVE_NOT_INITIALIZED:
            return "Enclave not initialized";
        case ERROR_ENCLAVE_ALREADY_INITIALIZED:
            return "Enclave already initialized";
        case ERROR_INITIALIZATION_FAILED:
            return "Initialization failed";
        case ERROR_CLEANUP_FAILED:
            return "Cleanup failed";
        
        // Cryptographic errors
        case ERROR_CRYPTO_INIT_FAILED:
            return "Cryptographic initialization failed";
        case ERROR_KEY_GENERATION_FAILED:
            return "Key generation failed";
        case ERROR_ENCRYPTION_FAILED:
            return "Encryption failed";
        case ERROR_DECRYPTION_FAILED:
            return "Decryption failed";
        case ERROR_SIGNATURE_FAILED:
            return "Digital signature failed";
        case ERROR_VERIFICATION_FAILED:
            return "Signature verification failed";
        case ERROR_INVALID_KEY:
            return "Invalid cryptographic key";
        case ERROR_INVALID_SIGNATURE:
            return "Invalid signature";
        case ERROR_HASH_FAILED:
            return "Hash operation failed";
        
        // Vote processing errors
        case ERROR_INVALID_VOTE:
            return "Invalid vote format";
        case ERROR_VOTE_ALREADY_PROCESSED:
            return "Vote already processed";
        case ERROR_VOTE_VERIFICATION_FAILED:
            return "Vote verification failed";
        case ERROR_INVALID_VOTER_ID:
            return "Invalid voter ID";
        case ERROR_INVALID_CANDIDATE_ID:
            return "Invalid candidate ID";
        case ERROR_VOTE_EXPIRED:
            return "Vote has expired";
        case ERROR_DUPLICATE_VOTE:
            return "Duplicate vote detected";
        
        // Storage errors
        case ERROR_SEAL_FAILED:
            return "Data sealing failed";
        case ERROR_UNSEAL_FAILED:
            return "Data unsealing failed";
        case ERROR_STORAGE_CORRUPTED:
            return "Storage data corrupted";
        case ERROR_STORAGE_NOT_FOUND:
            return "Storage data not found";
        case ERROR_STORAGE_WRITE_FAILED:
            return "Storage write failed";
        case ERROR_STORAGE_READ_FAILED:
            return "Storage read failed";
        
        // Network/IO errors
        case ERROR_NETWORK_FAILED:
            return "Network operation failed";
        case ERROR_FILE_NOT_FOUND:
            return "File not found";
        case ERROR_FILE_READ_FAILED:
            return "File read failed";
        case ERROR_FILE_WRITE_FAILED:
            return "File write failed";
        case ERROR_NETWORK_TIMEOUT:
            return "Network timeout";
        case ERROR_CONNECTION_FAILED:
            return "Connection failed";
        
        // Attestation errors
        case ERROR_ATTESTATION_FAILED:
            return "Attestation failed";
        case ERROR_QUOTE_GENERATION_FAILED:
            return "Quote generation failed";
        case ERROR_QUOTE_VERIFICATION_FAILED:
            return "Quote verification failed";
        case ERROR_CERTIFICATE_INVALID:
            return "Certificate invalid";
        case ERROR_PLATFORM_NOT_TRUSTED:
            return "Platform not trusted";
        
        // Aggregation errors
        case ERROR_AGGREGATION_FAILED:
            return "Vote aggregation failed";
        case ERROR_INSUFFICIENT_VOTES:
            return "Insufficient votes for aggregation";
        case ERROR_AGGREGATION_CORRUPTED:
            return "Aggregation data corrupted";
        case ERROR_PROOF_GENERATION_FAILED:
            return "Proof generation failed";
        case ERROR_PROOF_VERIFICATION_FAILED:
            return "Proof verification failed";
        
        // Big integer errors
        case ERROR_BIGINT_OVERFLOW:
            return "Big integer overflow";
        case ERROR_BIGINT_INVALID_OPERATION:
            return "Invalid big integer operation";
        case ERROR_BIGINT_DIVISION_BY_ZERO:
            return "Division by zero";
        case ERROR_BIGINT_CONVERSION_FAILED:
            return "Big integer conversion failed";
        
        // Simulation mode errors
        case ERROR_SIMULATION_NOT_SUPPORTED:
            return "Operation not supported in simulation mode";
        case ERROR_HARDWARE_FEATURE_REQUIRED:
            return "Hardware feature required";
        case ERROR_SIMULATION_CONFIGURATION_INVALID:
            return "Invalid simulation configuration";
        
        default:
            return "Unknown error code";
    }
}

// Helper function to get enclave error description
const char* get_enclave_error_description(enclave_result_t error_code) {
    switch (error_code) {
        case ENCLAVE_SUCCESS:
            return "Operation completed successfully";
        
        // General errors
        case ENCLAVE_ERROR_GENERAL:
            return "General enclave error";
        case ENCLAVE_ERROR_INVALID_PARAMETER:
            return "Invalid parameter provided";
        case ENCLAVE_ERROR_NULL_POINTER:
            return "Null pointer encountered";
        case ENCLAVE_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small for operation";
        case ENCLAVE_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case ENCLAVE_ERROR_NOT_INITIALIZED:
            return "Enclave not initialized";
        case ENCLAVE_ERROR_ALREADY_INITIALIZED:
            return "Enclave already initialized";
        case ENCLAVE_ERROR_OPERATION_FAILED:
            return "Operation failed";
        case ENCLAVE_ERROR_NOT_IMPLEMENTED:
            return "Feature not implemented";
        case ENCLAVE_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        
        // Initialization errors
        case ENCLAVE_ERROR_INITIALIZATION_FAILED:
            return "Initialization failed";
        case ENCLAVE_ERROR_CLEANUP_FAILED:
            return "Cleanup failed";
        case ENCLAVE_ERROR_ALREADY_RUNNING:
            return "Service already running";
        
        // Cryptographic errors
        case ENCLAVE_ERROR_CRYPTO_FAILED:
            return "Cryptographic operation failed";
        case ENCLAVE_ERROR_INVALID_SIGNATURE:
            return "Invalid signature";
        case ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED:
            return "Signature verification failed";
        case ENCLAVE_ERROR_KEY_GENERATION_FAILED:
            return "Key generation failed";
        case ENCLAVE_ERROR_INVALID_KEY_SIZE:
            return "Invalid key size";
        case ENCLAVE_ERROR_ENCRYPTION_FAILED:
            return "Encryption failed";
        case ENCLAVE_ERROR_DECRYPTION_FAILED:
            return "Decryption failed";
        case ENCLAVE_ERROR_HASH_FAILED:
            return "Hash computation failed";
        
        // Vote processing errors
        case ENCLAVE_ERROR_INVALID_VOTE_ID:
            return "Invalid vote ID";
        case ENCLAVE_ERROR_INVALID_CANDIDATE:
            return "Invalid candidate ID";
        case ENCLAVE_ERROR_INVALID_TIMESTAMP:
            return "Invalid timestamp";
        case ENCLAVE_ERROR_DUPLICATE_VOTE:
            return "Duplicate vote detected";
        case ENCLAVE_ERROR_VOTE_BUFFER_FULL:
            return "Vote buffer is full";
        case ENCLAVE_ERROR_INVALID_VOTE:
            return "Invalid vote";
        case ENCLAVE_ERROR_VOTE_VERIFICATION_FAILED:
            return "Vote verification failed";
        
        // Sealed storage errors
        case ENCLAVE_ERROR_SEALING_FAILED:
            return "Data sealing failed";
        case ENCLAVE_ERROR_UNSEALING_FAILED:
            return "Data unsealing failed";
        case ENCLAVE_ERROR_INVALID_SEALED_DATA:
            return "Invalid sealed data";
        case ENCLAVE_ERROR_SEALED_DATA_INTEGRITY:
            return "Sealed data integrity check failed";
        case ENCLAVE_ERROR_SEALED_DATA_TOO_LARGE:
            return "Sealed data too large";
        case ENCLAVE_ERROR_NO_SEALED_DATA:
            return "No sealed data found";
        
        // Network errors
        case ENCLAVE_ERROR_NETWORK_INIT:
            return "Network initialization failed";
        case ENCLAVE_ERROR_NETWORK_SOCKET:
            return "Network socket error";
        case ENCLAVE_ERROR_NETWORK_BIND:
            return "Network bind failed";
        case ENCLAVE_ERROR_NETWORK_LISTEN:
            return "Network listen failed";
        case ENCLAVE_ERROR_NETWORK_RECEIVE:
            return "Network receive failed";
        case ENCLAVE_ERROR_NETWORK_SEND:
            return "Network send failed";
        case ENCLAVE_ERROR_INVALID_HTTP_REQUEST:
            return "Invalid HTTP request";
        
        // Thread errors
        case ENCLAVE_ERROR_THREAD_CREATE:
            return "Thread creation failed";
        case ENCLAVE_ERROR_THREAD_JOIN:
            return "Thread join failed";
        
        // File I/O errors
        case ENCLAVE_ERROR_FILE_NOT_FOUND:
            return "File not found";
        case ENCLAVE_ERROR_FILE_READ_FAILED:
            return "File read failed";
        case ENCLAVE_ERROR_FILE_WRITE_FAILED:
            return "File write failed";
        case ENCLAVE_ERROR_FILE_PERMISSION:
            return "File permission denied";
        
        default:
            return "Unknown enclave error code";
    }
}
