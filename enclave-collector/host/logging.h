#ifndef LOGGING_H
#define LOGGING_H

#include "host_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_TRACE = 4
} log_level_t;

// Logging functions
int logging_init(const host_config_t* config);
void logging_cleanup(void);

void log_message(log_level_t level, const char* format, ...);
void log_error(const char* format, ...);
void log_warning(const char* format, ...);
void log_info(const char* format, ...);
void log_debug(const char* format, ...);
void log_trace(const char* format, ...);

// Log with context
void log_with_context(log_level_t level, const char* function, int line, const char* format, ...);

// Macros for convenient logging with context
#define LOG_ERROR(...) log_with_context(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) log_with_context(LOG_LEVEL_WARNING, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) log_with_context(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_with_context(LOG_LEVEL_DEBUG, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(...) log_with_context(LOG_LEVEL_TRACE, __FUNCTION__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGING_H
