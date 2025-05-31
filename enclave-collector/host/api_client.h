#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "shared_types.h"
#include "error_codes.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// API client configuration
typedef struct {
    char base_url[512];
    char auth_token[256];
    int timeout_ms;
    int max_retries;
} api_config_t;

// Forward declarations - actual definitions are in shared_types.h
// crypto_params_t and api_auxiliary_value_t are already defined in shared_types.h

// API client functions
enclave_result_t api_client_init(const api_config_t* config);
void api_client_cleanup(void);

// Essential collector endpoints (only 3 needed)
enclave_result_t api_fetch_system_params(crypto_params_t* params);
enclave_result_t api_fetch_auxiliary_values(api_auxiliary_value_t** values, size_t* count);
enclave_result_t api_submit_aux_product(const char* aux_product);

// Utility functions
void api_free_auxiliary_values(api_auxiliary_value_t* values, size_t count);

#ifdef __cplusplus
}
#endif

#endif // API_CLIENT_H
