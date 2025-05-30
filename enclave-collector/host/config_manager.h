#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "shared_types.h"
#include "api_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration file structure
typedef struct {
    api_config_t api_config;
    char log_level[32];
    char data_directory[512];
    int enable_tls;
    int enable_auth;
} system_config_t;

// Configuration loading functions
enclave_result_t config_load_from_file(const char* config_file_path, system_config_t* config);
enclave_result_t config_load_from_env(system_config_t* config);
enclave_result_t config_load_default(system_config_t* config);
enclave_result_t config_validate(const system_config_t* config);

// System initialization
enclave_result_t config_system_init(const char* config_file_path);
const system_config_t* config_get_current(void);

// Helper functions
const char* config_get_api_base_url(void);
const char* config_get_api_auth_token(void);
int config_get_api_timeout(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
