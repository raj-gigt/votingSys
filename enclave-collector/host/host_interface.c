#include "host_interface.h"
#include "crypto_processor.h"
#include "logging.h"
#include "file_operations.h"
#include "network_interface.h"
#include "api_client.h"
#include "config_manager.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#endif

#ifndef SIMULATION_MODE
// Include Open Enclave headers for production builds
#ifdef OE_BUILD_ENCLAVE
#include <openenclave/host.h>
#include "collector_u.h" // Generated from EDL
#endif
#endif

// Global simulation variables (available in all builds)
int g_sim_initialized = 0;
uint32_t g_next_vote_id = 1;
collector_state_t g_sim_state;
key_pair_t g_sim_keys;
// Note: No more static election parameters or keys - these come from external API

// Global crypto processor for secure mathematical operations
static crypto_processor_t g_crypto_processor = {0};

// Initialize host interface
enclave_result_t host_initialize(host_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_NULL_POINTER;
    }

    log_info("Initializing host interface...");

    // Clear context
    memset(context, 0, sizeof(host_context_t));
    context->session_id = (uint64_t)time(NULL);
    
    // Initialize configuration system first
    enclave_result_t config_result = config_system_init("./config/enclave_config.json");
    if (config_result != ENCLAVE_SUCCESS) {
        log_warning("Failed to load config file, using environment and defaults: %s", 
                   get_enclave_error_description(config_result));
        // Continue with defaults - config_system_init handles fallbacks
    }
    
    // Get current configuration
    const system_config_t* system_config = config_get_current();
    if (!system_config) {
        log_error("Failed to get system configuration");
        return ERROR_GENERAL_FAILURE;
    }
    
    // Store API configuration reference in context (no need to allocate separately)
    context->api_config = (api_config_t*)&system_config->api_config;
    
    // Initialize API client with current configuration
    enclave_result_t api_result = api_client_init(&system_config->api_config);
    if (api_result != ENCLAVE_SUCCESS) {
        log_error("Failed to initialize API client: %s", get_enclave_error_description(api_result));
        return ERROR_GENERAL_FAILURE;
    }

    log_info("API client initialized with base URL: %s", config_get_api_base_url());

    // Initialize file operations
    int result = file_operations_init(&context->config);
    if (result != SUCCESS) {
        log_error("Failed to initialize file operations: %s", get_error_description(result));
        return result;
    }

#ifdef SIMULATION_MODE
    log_info("Running in simulation mode");
    context->config.simulation_mode = 1;
    
    // Initialize simulation
    result = sim_initialize_collector(&context->collector_state);
    if (result != SUCCESS) {
        log_error("Failed to initialize simulation: %s", get_error_description(result));
        return result;
    }
#else
    // Initialize enclave for production builds
    if (!context->config.simulation_mode) {
        result = host_create_enclave(context);
        if (result != SUCCESS) {
            log_error("Failed to create enclave: %s", get_error_description(result));
            return result;
        }

        result = host_initialize_enclave(context);
        if (result != SUCCESS) {
            log_error("Failed to initialize enclave: %s", get_error_description(result));
            host_destroy_enclave(context);
            return result;
        }    }
#endif

    // Initialize the crypto processor for secure mathematical operations
    int crypto_result = crypto_processor_init(&g_crypto_processor, context);
    if (crypto_result != SUCCESS) {
        log_error("Failed to initialize crypto processor: %s", get_error_description(crypto_result));
        return crypto_result;
    }
      context->is_initialized = 1;
    log_info("Host interface initialized successfully with crypto processor");
    log_info("Ready to process auxiliary values: aux_i = pk_A^sk_i = H()^(sk_A * sk_i)");
    return ENCLAVE_SUCCESS;
}

// Cleanup host interface
enclave_result_t host_cleanup(host_context_t* context) {
    if (!context || !context->is_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    log_info("Cleaning up host interface...");

    // Cleanup crypto processor first
    crypto_processor_cleanup(&g_crypto_processor);

#ifndef SIMULATION_MODE
    if (!context->config.simulation_mode && context->enclave_handle) {
        host_destroy_enclave(context);
    }
#endif

    // Cleanup file operations
    file_operations_cleanup();    context->is_initialized = 0;
    log_info("Host interface cleaned up");
    return ENCLAVE_SUCCESS;
}

#ifndef SIMULATION_MODE
#ifdef OE_BUILD_ENCLAVE
// Create enclave instance
int host_create_enclave(host_context_t* context) {
    oe_result_t result = OE_OK;
    uint32_t enclave_flags = OE_ENCLAVE_FLAG_DEBUG;

    const char* enclave_path = context->config.enclave_path;
    if (strlen(enclave_path) == 0) {
        enclave_path = "collector_enclave.signed";
    }

    log_info("Creating enclave from: %s", enclave_path);

    result = oe_create_collector_enclave(
        enclave_path,
        OE_ENCLAVE_TYPE_SGX,
        enclave_flags,
        NULL,
        0,
        (oe_enclave_t**)&context->enclave_handle);

    if (result != OE_OK) {
        log_error("Failed to create enclave: %s", oe_result_str(result));
        return ERROR_INITIALIZATION_FAILED;
    }

    log_info("Enclave created successfully");
    return SUCCESS;
}

// Destroy enclave instance
int host_destroy_enclave(host_context_t* context) {
    if (context->enclave_handle) {
        oe_result_t result = oe_terminate_enclave((oe_enclave_t*)context->enclave_handle);
        if (result != OE_OK) {
            log_error("Failed to terminate enclave: %s", oe_result_str(result));
            return ERROR_CLEANUP_FAILED;
        }
        context->enclave_handle = NULL;
        log_info("Enclave destroyed");
    }
    return SUCCESS;
}

// Initialize enclave
int host_initialize_enclave(host_context_t* context) {
    oe_result_t result = OE_OK;
    int ecall_result = ERROR_GENERAL_FAILURE;

    result = ecall_initialize_collector(
        (oe_enclave_t*)context->enclave_handle,
        &ecall_result,
        &context->collector_state);

    if (result != OE_OK) {
        log_error("ECALL failed: %s", oe_result_str(result));
        return ERROR_INITIALIZATION_FAILED;
    }

    if (ecall_result != SUCCESS) {
        log_error("Enclave initialization failed: %s", get_error_description(ecall_result));
        return ecall_result;
    }

    log_info("Enclave initialized successfully");
    return SUCCESS;
}
#else
// Stub implementations when Open Enclave is not available
int host_create_enclave(host_context_t* context) {
    log_warning("Open Enclave not available, switching to simulation mode");
    context->config.simulation_mode = 1;
    return sim_initialize_collector(&context->collector_state);
}

int host_destroy_enclave(host_context_t* context) {
    return SUCCESS;
}

int host_initialize_enclave(host_context_t* context) {
    return SUCCESS;
}
#endif
#endif

// Process a single request
int host_process_request(host_context_t* context, const host_request_t* request, host_response_t* response) {
    if (!context || !request || !response) {
        return ERROR_NULL_POINTER;
    }

    if (!context->is_initialized) {
        return ERROR_ENCLAVE_NOT_INITIALIZED;
    }

    // Validate request
    int result = host_validate_request(request);
    if (result != SUCCESS) {
        return result;
    }

    // Initialize response
    memset(response, 0, sizeof(host_response_t));
    response->request_id = request->request_id;

    log_debug("Processing request type %d, ID %d", request->message_type, request->request_id);

    switch (request->message_type) {
        case MSG_TYPE_VOTE_SUBMISSION: {
            if (request->payload_size < sizeof(vote_t)) {
                response->result_code = ERROR_INVALID_PARAMETER;
                break;
            }
            
            vote_t* vote = (vote_t*)request->payload;
            uint32_t vote_id = 0;
            
            result = host_handle_vote_submission(context, vote, &vote_id);
            response->result_code = result;
            
            if (result == SUCCESS) {
                response->response_data = malloc(sizeof(uint32_t));
                if (response->response_data) {
                    memcpy(response->response_data, &vote_id, sizeof(uint32_t));
                    response->response_size = sizeof(uint32_t);
                } else {
                    response->result_code = ERROR_OUT_OF_MEMORY;
                }
            }
            break;
        }

        case MSG_TYPE_AGGREGATION_REQUEST: {
            aggregation_summary_t summary = {0};
            
            result = host_handle_aggregation_request(context, &summary);
            response->result_code = result;
            
            if (result == SUCCESS) {
                response->response_data = malloc(sizeof(aggregation_summary_t));
                if (response->response_data) {
                    memcpy(response->response_data, &summary, sizeof(aggregation_summary_t));
                    response->response_size = sizeof(aggregation_summary_t);
                } else {
                    response->result_code = ERROR_OUT_OF_MEMORY;
                }
            }
            break;
        }

        case MSG_TYPE_STATUS_REQUEST: {
            collector_state_t state = {0};
            
            result = host_handle_status_request(context, &state);
            response->result_code = result;
            
            if (result == SUCCESS) {
                response->response_data = malloc(sizeof(collector_state_t));
                if (response->response_data) {
                    memcpy(response->response_data, &state, sizeof(collector_state_t));
                    response->response_size = sizeof(collector_state_t);
                } else {
                    response->result_code = ERROR_OUT_OF_MEMORY;
                }
            }
            break;
        }

        default:
            log_warning("Unknown message type: %d", request->message_type);
            response->result_code = ERROR_NOT_IMPLEMENTED;
            break;
    }

    log_debug("Request %d processed with result: %s", 
              request->request_id, get_error_description(response->result_code));

    return SUCCESS;
}

// Handle vote submission
int host_handle_vote_submission(host_context_t* context, const vote_t* vote, uint32_t* vote_id) {
    if (!context || !vote || !vote_id) {
        return ERROR_NULL_POINTER;
    }

    // Convert vote_id to hex string for logging
    char vote_id_hex[VOTE_ID_SIZE * 2 + 1];
    for (int i = 0; i < VOTE_ID_SIZE; i++) {
        snprintf(vote_id_hex + i * 2, 3, "%02x", vote->vote_id[i]);
    }
    vote_id_hex[VOTE_ID_SIZE * 2] = '\0';
    
    log_debug("Processing vote submission for vote ID: %s", vote_id_hex);

#ifdef SIMULATION_MODE
    return sim_process_vote(vote, vote_id);
#else
    if (context->config.simulation_mode) {
        return sim_process_vote(vote, vote_id);
    }

#ifdef OE_BUILD_ENCLAVE
    oe_result_t result = OE_OK;
    int ecall_result = ERROR_GENERAL_FAILURE;

    result = ecall_process_vote(
        (oe_enclave_t*)context->enclave_handle,
        &ecall_result,
        vote,
        vote_id);

    if (result != OE_OK) {
        log_error("ECALL failed: %s", oe_result_str(result));
        return ERROR_OPERATION_FAILED;
    }

    return ecall_result;
#else
    return ERROR_NOT_IMPLEMENTED;
#endif
#endif
}

// Handle aggregation request - now with crypto processor integration
int host_handle_aggregation_request(host_context_t* context, aggregation_summary_t* summary) {
    if (!context || !summary) {
        return ERROR_NULL_POINTER;
    }

    log_info("Processing cryptographic aggregation request");
    log_info("Computing: aux = ∏(i=1 to n) aux_i = H()^(sk_A * Σ(i=1 to n) sk_i)");

    // Use the crypto processor to compute final aggregation
    aggregation_result_t crypto_result;
    int result = crypto_compute_final_aggregation(&g_crypto_processor, &crypto_result);
    
    if (result != SUCCESS) {
        log_error("Failed to compute cryptographic aggregation: %s", get_error_description(result));
        return result;
    }

    // Convert crypto result to summary format
    summary->candidate_count = 1; // For now, we have one aggregated result
    summary->total_votes_counted = crypto_result.total_users;
    
    // Fill in the first result with our aggregated data
    if (summary->candidate_count > 0) {
        summary->results[0].candidate_id = 1; // Placeholder candidate ID
        summary->results[0].vote_count = crypto_result.total_users;
    }
    
    // Copy aggregation proof
    summary->proof_size = sizeof(crypto_result.aggregation_proof);
    if (summary->proof_size <= MAX_SIGNATURE_SIZE) {
        memcpy(summary->aggregation_proof, crypto_result.aggregation_proof, summary->proof_size);
    } else {
        summary->proof_size = MAX_SIGNATURE_SIZE;
        memcpy(summary->aggregation_proof, crypto_result.aggregation_proof, MAX_SIGNATURE_SIZE);
    }

    log_info("Cryptographic aggregation completed successfully");
    log_info("Final result: aux computed from %d auxiliary values", crypto_result.total_users);    // Post result to backend API
    enclave_result_t post_result = post_aggregation_to_backend(context, &crypto_result);
    if (post_result != ENCLAVE_SUCCESS) {
        log_warning("Failed to post aggregation to backend: %s", get_enclave_error_description(post_result));
        // Don't fail the aggregation request even if backend posting fails
    }return SUCCESS;
}

// Handle status request
int host_handle_status_request(host_context_t* context, collector_state_t* state) {
    if (!context || !state) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Processing status request");

#ifdef SIMULATION_MODE
    memcpy(state, &g_sim_state, sizeof(collector_state_t));
    return SUCCESS;
#else
    if (context->config.simulation_mode) {
        memcpy(state, &g_sim_state, sizeof(collector_state_t));
        return SUCCESS;
    }

#ifdef OE_BUILD_ENCLAVE
    oe_result_t result = OE_OK;
    int ecall_result = ERROR_GENERAL_FAILURE;

    result = ecall_get_collector_state(
        (oe_enclave_t*)context->enclave_handle,
        &ecall_result,
        state);

    if (result != OE_OK) {
        log_error("ECALL failed: %s", oe_result_str(result));
        return ERROR_OPERATION_FAILED;
    }

    return ecall_result;
#else
    return ERROR_NOT_IMPLEMENTED;
#endif
#endif
}

// Process pending requests - now with real-time API fetching and crypto operations
int host_process_pending_requests(host_context_t* context) {
    if (!context) {
        return ERROR_NULL_POINTER;
    }

    // Check if we have a network context available
    if (!context->network_context) {
        return ERROR_NO_DATA; // No network to process
    }

    log_debug("Processing pending requests from API endpoints...");
    
    // Real-time implementation: Fetch auxiliary values from backend API
    api_auxiliary_value_t* api_aux_values = NULL;
    size_t aux_count = 0;
    
    // Step 1: Fetch auxiliary values from /api/collector/fetch-auxiliary
    enclave_result_t fetch_result = api_fetch_auxiliary_values_realtime(&api_aux_values, &aux_count);
    if (fetch_result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values from API: %s", get_enclave_error_description(fetch_result));
        return ERROR_OPERATION_FAILED;
    }
    
    if (aux_count == 0) {
        log_debug("No auxiliary values available from API");
        return SUCCESS; // No data to process
    }
    
    log_info("Fetched %zu auxiliary values from API for processing", aux_count);
    
    // Step 2: Process each auxiliary value through crypto operations
    int processed_count = 0;
    for (size_t i = 0; i < aux_count; i++) {
        // Convert API auxiliary value to crypto auxiliary value format
        auxiliary_value_t crypto_aux;
        memset(&crypto_aux, 0, sizeof(crypto_aux));
        
        // Parse voter ID from string to user_id (for crypto processor compatibility)
        crypto_aux.user_id = (uint32_t)(i + 1); // Use index as user ID for now
        crypto_aux.timestamp = (uint64_t)time(NULL);
          // Convert hex string to binary data for crypto processing
        uint8_t* binary_data = NULL;
        size_t binary_len = 0;
        
        int conv_result = hex_to_binary(api_aux_values[i].aux_value, &binary_data, &binary_len);
        if (conv_result == SUCCESS && binary_data) {
            crypto_aux.aux_i.data = binary_data;
            crypto_aux.aux_i.length = binary_len;
            
            // Generate signature (in production this would come from the client)
            memset(crypto_aux.signature, (i + 1) * 0x11, sizeof(crypto_aux.signature));
            
            // Step 3: Process through crypto processor for secure aggregation
            int crypto_result = crypto_process_auxiliary_value(&g_crypto_processor, &crypto_aux);
            if (crypto_result == SUCCESS) {
                processed_count++;
                log_info("Successfully processed auxiliary value %zu from voter: %s", 
                        i + 1, api_aux_values[i].voter_id);
            } else {
                log_warning("Failed to process auxiliary value %zu: %s", 
                           i + 1, get_error_description(crypto_result));
            }
            
            free(binary_data);
        } else {
            log_error("Failed to convert hex auxiliary value to binary for voter: %s", 
                     api_aux_values[i].voter_id);
        }
    }
    
    log_info("Real-time processing complete: %d/%zu auxiliary values processed successfully", 
             processed_count, aux_count);
    
    // Step 4: If we have processed values, compute and submit final aggregation
    if (processed_count > 0) {
        aggregation_result_t aggregation;
        memset(&aggregation, 0, sizeof(aggregation));
        
        int agg_result = crypto_compute_final_aggregation(&g_crypto_processor, &aggregation);
        if (agg_result == SUCCESS) {
            log_info("Successfully computed final aggregation result");
            
            // Convert binary result to hex string for API submission
            char product_hex[2048] = {0};
            if (aggregation.final_aux.length > 0 && aggregation.final_aux.data) {
                for (size_t i = 0; i < aggregation.final_aux.length && i < 1023; i++) {
                    snprintf(&product_hex[i * 2], 3, "%02x", 
                            ((uint8_t*)aggregation.final_aux.data)[i]);
                }
                
                // Step 5: Submit aggregation result to /api/collector/aux
                enclave_result_t submit_result = api_submit_aux_product(product_hex);
                if (submit_result == ENCLAVE_SUCCESS) {
                    log_info("Successfully submitted aggregation result to backend API");
                } else {
                    log_error("Failed to submit aggregation result: %s", 
                             get_enclave_error_description(submit_result));
                }
            } else {
                log_warning("Invalid aggregation result - empty data");
            }
        } else {
            log_error("Failed to compute final aggregation: %s", get_error_description(agg_result));
        }
    }
    
    // Clean up auxiliary values
    if (api_aux_values) {
        free(api_aux_values);
    }
    
    return SUCCESS;
}

// Perform maintenance tasks - new implementation
int host_perform_maintenance(host_context_t* context) {
    if (!context) {
        return ERROR_NULL_POINTER;
    }
    
    static time_t last_maintenance = 0;
    time_t current_time = time(NULL);
    
    // Run maintenance every 30 seconds
    if (current_time - last_maintenance < 30) {
        return SUCCESS;
    }
    
    last_maintenance = current_time;
    
    log_debug("Performing periodic maintenance...");
    
    // Maintenance tasks:
    // 1. Check enclave health
    // 2. Clean up expired sessions
    // 3. Verify cryptographic state integrity
    // 4. Report status to backend API
    
    collector_state_t state;
    if (host_handle_status_request(context, &state) == SUCCESS) {
        log_info("Maintenance: Collector has %d total votes (%d valid, %d invalid), sealed: %s",
                state.total_votes, state.valid_votes, state.invalid_votes,
                state.is_sealed ? "yes" : "no");
    }
    
    return SUCCESS;
}

// Validate incoming request
int host_validate_request(const host_request_t* request) {
    if (!request) {
        return ERROR_NULL_POINTER;
    }

    if (request->payload_size > MAX_MESSAGE_SIZE) {
        return ERROR_BUFFER_TOO_SMALL;
    }    // Check timestamp (within tolerance)
    uint64_t now = get_timestamp_ms();
    if (llabs((long long)(now - request->timestamp)) > TIMESTAMP_TOLERANCE_MS) {
        return ERROR_VOTE_EXPIRED;
    }

    return SUCCESS;
}

// Get current timestamp in milliseconds
uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return time / 10000; // Convert from 100ns to ms
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

#ifdef SIMULATION_MODE

// Simulation mode implementations
int sim_initialize_collector(collector_state_t* state) {
    if (!state) {
        return ERROR_NULL_POINTER;
    }

    log_info("Initializing simulation collector");

    // Initialize simulation state
    memset(&g_sim_state, 0, sizeof(collector_state_t));
    g_sim_state.is_initialized = 1;
    
    // Generate simulation keys
    int result = sim_generate_keys(&g_sim_keys);
    if (result != SUCCESS) {
        log_error("Failed to generate simulation keys: %s", get_error_description(result));
        return result;
    }

    // Copy public key to state
    memcpy(g_sim_state.public_key, g_sim_keys.public_key, g_sim_keys.public_key_size);
    g_sim_state.public_key_size = g_sim_keys.public_key_size;

    g_sim_initialized = 1;
    g_next_vote_id = 1;

    // Copy state back
    memcpy(state, &g_sim_state, sizeof(collector_state_t));

    log_info("Simulation collector initialized with key size: %zu", state->public_key_size);
    return SUCCESS;
}

int sim_process_vote(const vote_t* vote, uint32_t* vote_id) {
    if (!vote || !vote_id) {
        return ERROR_NULL_POINTER;
    }    if (!g_sim_initialized) {
        return ERROR_ENCLAVE_NOT_INITIALIZED;
    }

    // Convert vote_id to hex string for logging
    char vote_id_hex[VOTE_ID_SIZE * 2 + 1];
    for (int i = 0; i < VOTE_ID_SIZE; i++) {
        snprintf(vote_id_hex + i * 2, 3, "%02x", vote->vote_id[i]);
    }
    vote_id_hex[VOTE_ID_SIZE * 2] = '\0';

    log_debug("Processing vote in simulation mode for vote ID: %s", vote_id_hex);

    // Basic vote validation
    bool vote_id_empty = true;
    for (int i = 0; i < VOTE_ID_SIZE; i++) {
        if (vote->vote_id[i] != 0) {
            vote_id_empty = false;
            break;
        }
    }
    if (vote_id_empty) {
        return ERROR_INVALID_VOTE_ID;
    }

    if (vote->candidate_id == 0) {
        return ERROR_INVALID_CANDIDATE_ID;
    }

    // Check timestamp (simple validation)
    uint64_t now = get_timestamp_ms();
    if (vote->timestamp > now + TIMESTAMP_TOLERANCE_MS || 
        vote->timestamp < now - MAX_VOTE_AGE_MS) {
        return ERROR_VOTE_EXPIRED;
    }

    // Simulate signature verification
    int is_valid = 0;
    int result = sim_verify_signature(
        (uint8_t*)vote, sizeof(vote_t) - sizeof(vote->signature) - sizeof(vote->signature_size),
        vote->signature, vote->signature_size,
        g_sim_state.public_key, g_sim_state.public_key_size,
        &is_valid
    );

    if (result != SUCCESS || !is_valid) {
        log_warning("Vote signature verification failed for voter: %s", vote->voter_id);
        g_sim_state.invalid_votes++;
        return ERROR_VERIFICATION_FAILED;
    }

    // Assign vote ID and update counters
    *vote_id = g_next_vote_id++;
    g_sim_state.total_votes++;
    g_sim_state.valid_votes++;

    log_debug("Vote processed successfully: ID=%d, Voter=%s, Candidate=%d", 
              *vote_id, vote->voter_id, vote->candidate_id);

    return SUCCESS;
}

int sim_aggregate_votes(aggregation_summary_t* summary) {
    if (!summary) {
        return ERROR_NULL_POINTER;
    }

    if (!g_sim_initialized) {
        return ERROR_ENCLAVE_NOT_INITIALIZED;
    }

    log_info("Performing vote aggregation in simulation mode");

    // Clear summary
    memset(summary, 0, sizeof(aggregation_summary_t));

    // Simulate aggregation results
    // In a real implementation, this would aggregate actual votes
    summary->candidate_count = 3; // Example: 3 candidates
    summary->results[0].candidate_id = 1;
    summary->results[0].vote_count = g_sim_state.valid_votes / 3;
    summary->results[1].candidate_id = 2;
    summary->results[1].vote_count = g_sim_state.valid_votes / 3;
    summary->results[2].candidate_id = 3;
    summary->results[2].vote_count = g_sim_state.valid_votes - (summary->results[0].vote_count + summary->results[1].vote_count);

    summary->total_votes_counted = g_sim_state.valid_votes;

    // Generate aggregation proof (simulated)
    const char* proof_data = "SIMULATION_AGGREGATION_PROOF";
    size_t proof_len = strlen(proof_data);
    if (proof_len < MAX_SIGNATURE_SIZE) {
        memcpy(summary->aggregation_proof, proof_data, proof_len);
        summary->proof_size = proof_len;
    }

    log_info("Aggregation complete: %llu total votes, %zu candidates", 
             summary->total_votes_counted, summary->candidate_count);

    return SUCCESS;
}

int sim_generate_keys(key_pair_t* keys) {
    if (!keys) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Generating simulation keys");

    // Generate simple simulation keys (not cryptographically secure)
    const char* sim_private_key = "SIMULATION_PRIVATE_KEY_1234567890ABCDEF";
    const char* sim_public_key = "SIMULATION_PUBLIC_KEY_1234567890ABCDEF";

    size_t priv_len = strlen(sim_private_key);
    size_t pub_len = strlen(sim_public_key);

    if (priv_len >= MAX_PRIVATE_KEY_SIZE || pub_len >= MAX_PUBLIC_KEY_SIZE) {
        return ERROR_BUFFER_TOO_SMALL;
    }

    memcpy(keys->private_key, sim_private_key, priv_len);
    keys->private_key_size = priv_len;
    memcpy(keys->public_key, sim_public_key, pub_len);
    keys->public_key_size = pub_len;

    log_debug("Simulation keys generated: pub_size=%zu, priv_size=%zu", 
              keys->public_key_size, keys->private_key_size);

    return SUCCESS;
}

int sim_encrypt_data(const uint8_t* plaintext, size_t data_size, encrypted_data_t* encrypted) {
    if (!plaintext || !encrypted || data_size == 0) {
        return ERROR_NULL_POINTER;
    }

    if (data_size > MAX_ENCRYPTED_DATA_SIZE - 16) { // Leave room for "encryption" overhead
        return ERROR_BUFFER_TOO_SMALL;
    }

    log_debug("Simulating data encryption of %zu bytes", data_size);

    // Simple XOR "encryption" for simulation
    const uint8_t sim_key = 0xAB;
    for (size_t i = 0; i < data_size; i++) {
        encrypted->data[i] = plaintext[i] ^ sim_key;
    }
    encrypted->data_size = data_size;

    // Set dummy IV and tag
    memset(encrypted->iv, 0x12, sizeof(encrypted->iv));
    memset(encrypted->tag, 0x34, sizeof(encrypted->tag));

    return SUCCESS;
}

int sim_decrypt_data(const encrypted_data_t* encrypted, uint8_t* plaintext, size_t* buffer_size) {
    if (!encrypted || !plaintext || !buffer_size) {
        return ERROR_NULL_POINTER;
    }

    if (*buffer_size < encrypted->data_size) {
        *buffer_size = encrypted->data_size;
        return ERROR_BUFFER_TOO_SMALL;
    }

    log_debug("Simulating data decryption of %zu bytes", encrypted->data_size);

    // Simple XOR "decryption" for simulation
    const uint8_t sim_key = 0xAB;
    for (size_t i = 0; i < encrypted->data_size; i++) {
        plaintext[i] = encrypted->data[i] ^ sim_key;
    }
    *buffer_size = encrypted->data_size;

    return SUCCESS;
}

int sim_sign_data(const uint8_t* data, size_t data_size, uint8_t* signature, size_t* sig_size) {
    if (!data || !signature || !sig_size || data_size == 0) {
        return ERROR_NULL_POINTER;
    }

    const char* sim_signature = "SIM_SIGNATURE_";
    size_t base_sig_len = strlen(sim_signature);
    
    if (*sig_size < base_sig_len + 8) {
        *sig_size = base_sig_len + 8;
        return ERROR_BUFFER_TOO_SMALL;
    }

    log_debug("Simulating data signing of %zu bytes", data_size);

    // Create a simple "signature" based on data hash simulation
    memcpy(signature, sim_signature, base_sig_len);
    
    // Add a simple checksum as part of signature
    uint32_t checksum = 0;
    for (size_t i = 0; i < data_size; i++) {
        checksum += data[i];
    }
    
    memcpy(signature + base_sig_len, &checksum, sizeof(checksum));
    *sig_size = base_sig_len + sizeof(checksum);

    return SUCCESS;
}

int sim_verify_signature(const uint8_t* data, size_t data_size, const uint8_t* signature, size_t sig_size, const uint8_t* public_key, size_t key_size, int* is_valid) {
    if (!data || !signature || !public_key || !is_valid || data_size == 0) {
        return ERROR_NULL_POINTER;
    }

    log_debug("Simulating signature verification of %zu bytes", data_size);

    *is_valid = 0;

    const char* sim_signature = "SIM_SIGNATURE_";
    size_t base_sig_len = strlen(sim_signature);
    
    if (sig_size < base_sig_len + sizeof(uint32_t)) {
        return ERROR_INVALID_SIGNATURE;
    }

    // Verify signature prefix
    if (memcmp(signature, sim_signature, base_sig_len) != 0) {
        return ERROR_INVALID_SIGNATURE;
    }

    // Verify checksum
    uint32_t expected_checksum = 0;
    for (size_t i = 0; i < data_size; i++) {
        expected_checksum += data[i];
    }

    uint32_t signature_checksum;
    memcpy(&signature_checksum, signature + base_sig_len, sizeof(uint32_t));

    *is_valid = (expected_checksum == signature_checksum) ? 1 : 0;

    log_debug("Signature verification result: %s", *is_valid ? "VALID" : "INVALID");
    return SUCCESS;
}

#endif // SIMULATION_MODE

// Note: Duplicate function definitions removed to fix compilation errors

// Get enclave information
enclave_result_t host_get_enclave_info(enclave_info_t* info) {
    if (!info) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

#ifdef SIMULATION_MODE
    if (!g_sim_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    strncpy(info->version, "1.0.0-sim", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    info->total_votes = g_sim_state.total_votes;
    info->valid_votes = g_sim_state.valid_votes;
    info->invalid_votes = g_sim_state.invalid_votes;
    info->is_sealed = g_sim_state.is_sealed;
#else
    // In production mode, this would make an ECALL to the enclave
    strncpy(info->version, "1.0.0", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    info->total_votes = 0;
    info->valid_votes = 0;
    info->invalid_votes = 0;
    info->is_sealed = false;
#endif

    return ENCLAVE_SUCCESS;
}

// Generate cryptographic keypair
enclave_result_t host_generate_keypair(crypto_key_t* public_key, crypto_key_t* private_key) {
    if (!public_key || !private_key) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

#ifdef SIMULATION_MODE
    if (!g_sim_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    // Copy simulation keys
    memcpy(public_key->data, g_sim_keys.public_key, CRYPTO_KEY_SIZE);
    public_key->size = CRYPTO_KEY_SIZE;
    
    memcpy(private_key->data, g_sim_keys.private_key, CRYPTO_KEY_SIZE);
    private_key->size = CRYPTO_KEY_SIZE;
#else
    // In production mode, this would make an ECALL to the enclave
    return ENCLAVE_ERROR_NOT_IMPLEMENTED;
#endif

    return ENCLAVE_SUCCESS;
}

// Process vote request
enclave_result_t host_process_vote_request(const vote_t* vote, vote_receipt_t* receipt) {
    if (!vote || !receipt) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

#ifdef SIMULATION_MODE
    if (!g_sim_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    // Simple validation
    if (vote->candidate_id >= MAX_CANDIDATES) {
        return ENCLAVE_ERROR_INVALID_CANDIDATE;
    }

    // Generate receipt
    memcpy(receipt->vote_id, vote->vote_id, VOTE_ID_SIZE);
    receipt->timestamp = vote->timestamp;
    receipt->status = VOTE_STATUS_ACCEPTED;
    
    // Simple hash computation
    uint32_t hash = 0;
    for (size_t i = 0; i < sizeof(vote_t); i++) {
        hash = ((hash << 5) + hash) + ((uint8_t*)vote)[i];
    }
    
    for (int i = 0; i < HASH_SIZE && i < 4; i++) {
        receipt->receipt_hash[i] = (uint8_t)((hash >> (i * 8)) & 0xFF);
    }
    
    // Update simulation state
    g_sim_state.total_votes++;
    g_sim_state.valid_votes++;
    g_sim_state.aggregation.candidate_votes[vote->candidate_id]++;
    g_sim_state.aggregation.total_votes++;
#else
    // In production mode, this would make an ECALL to the enclave
    return ENCLAVE_ERROR_NOT_IMPLEMENTED;
#endif

    return ENCLAVE_SUCCESS;
}

// Get vote aggregation
enclave_result_t host_get_vote_aggregation(vote_aggregation_t* aggregation) {
    if (!aggregation) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

#ifdef SIMULATION_MODE
    if (!g_sim_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }

    memcpy(aggregation, &g_sim_state.aggregation, sizeof(vote_aggregation_t));
#else
    // In production mode, this would make an ECALL to the enclave
    memset(aggregation, 0, sizeof(vote_aggregation_t));
#endif

    return ENCLAVE_SUCCESS;
}

// Additional integration test functions

enclave_result_t host_process_vote(const vote_t* vote, vote_receipt_t* receipt) {
    // This function is an alias for host_process_vote_request
    return host_process_vote_request(vote, receipt);
}

enclave_result_t host_aggregate_votes(vote_aggregation_t* aggregation) {
    // This function is an alias for host_get_vote_aggregation
    return host_get_vote_aggregation(aggregation);
}

// Initialize election context with parameters from external API
enclave_result_t host_initialize_election(host_context_t* context, const char* election_id) {
    if (!context || !election_id) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->is_initialized) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
    
    log_info("Initializing election context for election: %s", election_id);
    
    // Store current election ID
    strncpy(context->current_election_id, election_id, sizeof(context->current_election_id) - 1);
    context->current_election_id[sizeof(context->current_election_id) - 1] = '\0';
      // Fetch election parameters from external API
    crypto_params_t params;
    enclave_result_t result = api_fetch_election_params(election_id, &params);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch election parameters: %s", get_enclave_error_description(result));
        return result;
    }
    
    log_info("Successfully fetched election parameters");
    log_debug("N parameter: %.64s...", params.N);
    log_debug("H parameter: %.64s...", params.H);
    
#ifdef SIMULATION_MODE
    // In simulation mode, store parameters and initialize math context
    // (In production, this would be sent to the enclave)
    
    // Initialize secure math context (allocated on heap, not static)
    void* math_ctx = malloc(sizeof(math_context_t));
    if (!math_ctx) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    context->math_context = math_ctx;
    
    // Note: We would include secure_math.h in a real implementation
    // For now, just log that we would initialize with external parameters
    log_info("Would initialize secure math context with external parameters");
    
#else
    // In production mode, send parameters to enclave via ECALL
    log_info("Would send election parameters to enclave via ECALL");
#endif
    
    return ENCLAVE_SUCCESS;
}

// Process auxiliary values from external API
enclave_result_t host_process_auxiliary_values(host_context_t* context) {
    if (!context) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    if (!context->is_initialized || strlen(context->current_election_id) == 0) {
        return ENCLAVE_ERROR_NOT_INITIALIZED;
    }
      log_info("Processing auxiliary values for election: %s", context->current_election_id);
    
    // Fetch auxiliary values from external API
    api_auxiliary_value_t* aux_values;
    size_t aux_count;
    enclave_result_t result = api_fetch_auxiliary_values(context->current_election_id, &aux_values, &aux_count);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to fetch auxiliary values: %s", get_enclave_error_description(result));
        return result;
    }
    
    log_info("Fetched %zu auxiliary values from external API", aux_count);
    
#ifdef SIMULATION_MODE
    // Process auxiliary values through secure math operations
    for (size_t i = 0; i < aux_count; i++) {
        log_debug("Processing auxiliary value from voter: %s", aux_values[i].voter_id);
        
        // In a real implementation, would call secure_math_process_auxiliary
        // For now, just log the processing
        log_debug("Would process auxiliary value: %.32s...", aux_values[i].aux_value);
    }
    
    // Mock product result for now
    char product_hex[1024] = "deadbeefcafebabe0123456789abcdef";
    
    // Submit product back to external API
    result = api_submit_auxiliary_product(context->current_election_id, product_hex);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to submit auxiliary product: %s", get_enclave_error_description(result));
    } else {
        log_info("Successfully submitted auxiliary product to external API");
    }
    
#else
    // In production mode, send auxiliary values to enclave for processing
    log_info("Would send auxiliary values to enclave for secure processing");
#endif
    
    // Clean up auxiliary values
    api_free_auxiliary_values(aux_values, aux_count);
    
    return result;
}

// Store encryption keys externally (no static storage in enclave)
enclave_result_t host_store_enclave_keys(host_context_t* context, const char* key_type) {
    if (!context || !key_type) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    log_info("Storing enclave keys externally for type: %s", key_type);
    
    // Generate or retrieve keys from enclave
    crypto_key_t key;
    memset(&key, 0, sizeof(key));
    
#ifdef SIMULATION_MODE
    // Mock key generation
    key.type = CRYPTO_KEY_TYPE_AES;
    key.size = 32;
    for (size_t i = 0; i < key.size; i++) {
        key.data[i] = (uint8_t)(0x42 + i); // Mock key data
    }
#else
    // In production, generate keys securely in enclave
    log_info("Would generate keys securely in enclave");
#endif
    
    // Store key externally via API
    char key_id[256];
    snprintf(key_id, sizeof(key_id), "%s_%s_%lu", 
             context->current_election_id, key_type, context->session_id);
    
    enclave_result_t result = api_store_enclave_key(key_id, &key);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to store key externally: %s", get_enclave_error_description(result));
        return result;
    }
    
    log_info("Successfully stored key externally with ID: %s", key_id);
    return ENCLAVE_SUCCESS;
}

// Retrieve keys from external storage
enclave_result_t host_retrieve_enclave_keys(host_context_t* context, const char* key_type, crypto_key_t* key) {
    if (!context || !key_type || !key) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }
    
    char key_id[256];
    snprintf(key_id, sizeof(key_id), "%s_%s_%lu", 
             context->current_election_id, key_type, context->session_id);
    
    log_info("Retrieving key from external storage: %s", key_id);
    
    enclave_result_t result = api_fetch_enclave_key(key_id, key);
    if (result != ENCLAVE_SUCCESS) {
        log_error("Failed to retrieve key: %s", get_enclave_error_description(result));
        return result;
    }
    
    log_info("Successfully retrieved key from external storage");
    return ENCLAVE_SUCCESS;
}

// Utility function for hex string to binary conversion
int hex_to_binary(const char* hex_str, uint8_t** out_binary, size_t* out_length) {
    if (!hex_str || !out_binary || !out_length) {
        return ERROR_INVALID_PARAMETER;
    }
    
    size_t hex_len = strlen(hex_str);
    if (hex_len == 0 || hex_len % 2 != 0) {
        log_error("Invalid hex string length: %zu", hex_len);
        return ERROR_INVALID_PARAMETER;
    }
    
    size_t binary_len = hex_len / 2;    uint8_t* binary_data = malloc(binary_len);
    if (!binary_data) {
        log_error("Failed to allocate memory for binary conversion");
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }
    
    // Convert each pair of hex characters to a byte
    for (size_t i = 0; i < binary_len; i++) {
        char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
        char* endptr;
        unsigned long byte_val = strtoul(hex_byte, &endptr, 16);
        
        if (endptr != hex_byte + 2) {
            log_error("Invalid hex character at position %zu", i * 2);
            free(binary_data);
            return ERROR_INVALID_PARAMETER;
        }
        
        binary_data[i] = (uint8_t)byte_val;
    }
    
    *out_binary = binary_data;
    *out_length = binary_len;
    return SUCCESS;
}
