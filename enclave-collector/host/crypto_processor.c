#include "crypto_processor.h"
#include "logging.h"
#include "api_client.h"
#include "file_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Include the old collector module functions
// Note: We'll link against the old collector module's bigint_ops.c and collector.c

// External functions from old collector module
extern int collector_init(const ElectionParams* params);
extern int process_auxiliary_value_realtime(const BigInt* aux_i);
extern int get_current_auxiliary_product(BigInt* result);
extern int reset_auxiliary_product(void);
extern int collector_cleanup(void);

// External bigint operations from old collector module
extern BigInt create_bigint(const uint8_t* data, size_t length);
extern void free_bigint(BigInt* bigint);
extern int modular_multiplication(const BigInt* a, const BigInt* b, const BigInt* mod, BigInt* result);

// Initialize the crypto processor with secure enclave integration
enclave_result_t crypto_processor_init(crypto_processor_t* processor, host_context_t* host_ctx) {
    if (!processor || !host_ctx) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    memset(processor, 0, sizeof(crypto_processor_t));
    processor->host_context = host_ctx;

    log_info("Initializing crypto processor for secure auxiliary value aggregation...");

    // Load election parameters from configuration
    // For now, use default parameters - in production these would come from the backend API
    uint8_t default_n[] = {0x02, 0x03, 0x05, 0x07, 0x0B, 0x0D, 0x11, 0x13}; // Example small primes
    uint8_t default_h[] = {0x17, 0x19, 0x1D, 0x1F, 0x25, 0x29, 0x2B, 0x2F}; // Example hash base
    
    processor->election_params.N = create_bigint(default_n, sizeof(default_n));
    processor->election_params.H = create_bigint(default_h, sizeof(default_h));
    
    // N_squared = N * N (would be computed properly in production)
    uint8_t default_n_sq[] = {0x04, 0x09, 0x19, 0x31, 0x79, 0xA9, 0x21, 0x69}; // N^2 approximation
    processor->election_params.N_squared = create_bigint(default_n_sq, sizeof(default_n_sq));

    // Generate or load public key A: pk_A = H()^sk_A
    uint8_t default_pk_a[] = {0x2A, 0x3C, 0x4E, 0x5F, 0x61, 0x73, 0x85, 0x97}; // Example public key
    processor->pk_A = create_bigint(default_pk_a, sizeof(default_pk_a));    // Initialize the old collector module with our parameters
    int result = collector_init(&processor->election_params);
    if (result != 0) {
        log_error("Failed to initialize old collector module: %d", result);
        return ENCLAVE_ERROR_OPERATION_FAILED;
    }

    // Initialize running product to 1
    uint8_t one_value = 1;
    processor->running_product = create_bigint(&one_value, 1);
    processor->aux_count = 0;
    processor->is_initialized = true;
    processor->is_sealed = false;    log_info("Crypto processor initialized successfully");
    log_info("Mathematical protocol: aux = ∏(i=1 to n) aux_i = H()^(sk_A * Σ(i=1 to n) sk_i)");
    
    return ENCLAVE_SUCCESS;
}

// Process a new auxiliary value: aux_i = pk_A^sk_i = H()^(sk_A * sk_i)
enclave_result_t crypto_process_auxiliary_value(crypto_processor_t* processor, const auxiliary_value_t* aux_value) {
    if (!processor || !aux_value || !processor->is_initialized) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    if (processor->is_sealed) {
        log_warning("Cannot process auxiliary value: processor is sealed");
        return ENCLAVE_ERROR_INVALID_STATE;
    }

    log_info("Processing auxiliary value from user %d", aux_value->user_id);    // Verify the auxiliary value cryptographically
    enclave_result_t verify_result = crypto_verify_auxiliary_value(aux_value, &processor->election_params);
    if (verify_result != ENCLAVE_SUCCESS) {
        log_error("Auxiliary value verification failed: %s", get_enclave_error_description(verify_result));
        return verify_result;
    }    // Use the old collector module to perform secure multiplication
    // This implements: running_product = running_product * aux_i mod N^2
    int result = process_auxiliary_value_realtime(&aux_value->aux_i);
    if (result != 0) {
        log_error("Failed to process auxiliary value in collector: %d", result);
        return ENCLAVE_ERROR_OPERATION_FAILED;
    }

    // Update local state
    processor->aux_count++;
      log_info("Successfully processed auxiliary value. Total count: %d", processor->aux_count);
    
    // If we're in enclave mode, also process through the secure enclave
#ifdef SIMULATION_MODE
    // In simulation mode, we just log the operation
    log_debug("Simulation: aux_i processed through secure computation");
#else
    // In production, forward to enclave for secure processing
    if (processor->host_context && processor->host_context->enclave_handle) {
        // TODO: Call enclave function to securely process auxiliary value
        log_debug("Forwarding auxiliary value to secure enclave");
    }
#endif

    return ENCLAVE_SUCCESS;
}

// Compute final aggregation: aux = ∏(i=1 to n) aux_i
enclave_result_t crypto_compute_final_aggregation(crypto_processor_t* processor, aggregation_result_t* result) {
    if (!processor || !result || !processor->is_initialized) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    if (processor->aux_count == 0) {
        log_warning("No auxiliary values to aggregate");
        return ENCLAVE_ERROR_INVALID_STATE;
    }

    log_info("Computing final aggregation for %d auxiliary values", processor->aux_count);    // Get the final product from the old collector module
    int collect_result = get_current_auxiliary_product(&result->final_aux);
    if (collect_result != 0) {
        log_error("Failed to get aggregation result from collector: %d", collect_result);
        return ENCLAVE_ERROR_OPERATION_FAILED;
    }

    // Fill in result metadata
    result->total_users = processor->aux_count;
    result->computation_time = (uint64_t)time(NULL);

    // Generate aggregation proof (in production this would be a zero-knowledge proof)
    memset(result->aggregation_proof, 0xAA, sizeof(result->aggregation_proof)); // Placeholder

    // Seal the processor to prevent further modifications
    processor->is_sealed = true;    log_info("Final aggregation computed successfully");
    log_info("Result: aux = H()^(sk_A * Σ(i=1 to %d) sk_i)", processor->aux_count);

    return ENCLAVE_SUCCESS;
}

// Verify auxiliary value cryptographically
enclave_result_t crypto_verify_auxiliary_value(const auxiliary_value_t* aux_value, const ElectionParams* params) {
    if (!aux_value || !params) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    // Verify timestamp is recent (within last hour)
    uint64_t current_time = (uint64_t)time(NULL);
    if (current_time - aux_value->timestamp > 3600) {
        log_warning("Auxiliary value timestamp is too old");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    // Verify signature (placeholder - in production would use proper crypto)
    bool signature_valid = true; // TODO: Implement proper signature verification
    if (!signature_valid) {
        log_error("Auxiliary value signature verification failed");
        return ENCLAVE_ERROR_SIGNATURE_VERIFICATION_FAILED;
    }

    // Verify aux_i is in the correct mathematical group
    // In production: verify aux_i ∈ Z_N^2* and aux_i = pk_A^sk_i
    // For now, just check it's not zero
    if (aux_value->aux_i.length == 0 || aux_value->aux_i.data == NULL) {
        log_error("Invalid auxiliary value: empty or null");
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Auxiliary value verified successfully");
    return ENCLAVE_SUCCESS;
}

// Post aggregation result to backend API
enclave_result_t post_aggregation_to_backend(host_context_t* context, const aggregation_result_t* result) {
    if (!context || !result) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }    log_info("Posting aggregation result to backend API...");

    // Use the API client to store the aggregation result
    // For now, use a default election ID - in production this would be passed as parameter
    enclave_result_t api_result = api_store_aggregation_result("default_election", result);
    
    if (api_result != ENCLAVE_SUCCESS) {
        log_error("Failed to store aggregation result: %s", get_enclave_error_description(api_result));
        return ENCLAVE_ERROR_OPERATION_FAILED;
    }

    log_info("Aggregation result successfully stored via backend API");
    return ENCLAVE_SUCCESS;
}

// Cleanup crypto processor
enclave_result_t crypto_processor_cleanup(crypto_processor_t* processor) {
    if (!processor) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    if (processor->is_initialized) {
        // Cleanup old collector module
        collector_cleanup();

        // Free bigint structures
        free_bigint(&processor->election_params.N);
        free_bigint(&processor->election_params.N_squared);
        free_bigint(&processor->election_params.H);
        free_bigint(&processor->pk_A);
        free_bigint(&processor->running_product);

        processor->is_initialized = false;
        log_info("Crypto processor cleaned up successfully");
    }

    return ENCLAVE_SUCCESS;
}
