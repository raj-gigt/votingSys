#include "logging.h"
#include "constants.h"
#include "error_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#endif

// Global logging state
static struct {
    FILE* log_file;
    log_level_t current_level;
    char log_filename[256];
    int console_output;
    int initialized;
#ifdef _WIN32
    CRITICAL_SECTION log_mutex;
#else
    pthread_mutex_t log_mutex;
#endif
} g_log_state = {0};

// Log level names
static const char* log_level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

// Initialize logging system
int logging_init(const host_config_t* config) {
    if (g_log_state.initialized) {
        return SUCCESS;
    }

    // Initialize mutex
#ifdef _WIN32
    InitializeCriticalSection(&g_log_state.log_mutex);
#else
    if (pthread_mutex_init(&g_log_state.log_mutex, NULL) != 0) {
        return ERROR_INITIALIZATION_FAILED;
    }
#endif

    // Set log level
    g_log_state.current_level = (log_level_t)config->log_level;
    g_log_state.console_output = 1; // Always output to console

    // Set log filename
    if (strlen(config->log_file) > 0) {
        strncpy(g_log_state.log_filename, config->log_file, sizeof(g_log_state.log_filename) - 1);
    } else {
        strcpy(g_log_state.log_filename, "logs/collector.log");
    }

    // Create logs directory if it doesn't exist
    char dir_path[256];
    strncpy(dir_path, g_log_state.log_filename, sizeof(dir_path) - 1);
    char* last_slash = strrchr(dir_path, '/');
    if (!last_slash) {
        last_slash = strrchr(dir_path, '\\');
    }
    
    if (last_slash) {
        *last_slash = '\0';
#ifdef _WIN32
        CreateDirectoryA(dir_path, NULL);
#else
        mkdir(dir_path, 0755);
#endif
    }

    // Open log file
    g_log_state.log_file = fopen(g_log_state.log_filename, "a");
    if (!g_log_state.log_file) {
        fprintf(stderr, "Warning: Could not open log file %s, logging to console only\n", 
                g_log_state.log_filename);
    }

    g_log_state.initialized = 1;

    log_info("Logging system initialized");
    log_info("Log level: %s", log_level_names[g_log_state.current_level]);
    log_info("Log file: %s", g_log_state.log_filename);

    return SUCCESS;
}

// Cleanup logging system
void logging_cleanup(void) {
    if (!g_log_state.initialized) {
        return;
    }

    log_info("Shutting down logging system");

#ifdef _WIN32
    EnterCriticalSection(&g_log_state.log_mutex);
#else
    pthread_mutex_lock(&g_log_state.log_mutex);
#endif

    if (g_log_state.log_file) {
        fclose(g_log_state.log_file);
        g_log_state.log_file = NULL;
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_log_state.log_mutex);
    DeleteCriticalSection(&g_log_state.log_mutex);
#else
    pthread_mutex_unlock(&g_log_state.log_mutex);
    pthread_mutex_destroy(&g_log_state.log_mutex);
#endif

    g_log_state.initialized = 0;
}

// Get current timestamp string
static void get_timestamp_string(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Core logging function
static void log_internal(log_level_t level, const char* function, int line, const char* format, va_list args) {
    if (!g_log_state.initialized || level > g_log_state.current_level) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_log_state.log_mutex);
#else
    pthread_mutex_lock(&g_log_state.log_mutex);
#endif

    // Get timestamp
    char timestamp[32];
    get_timestamp_string(timestamp, sizeof(timestamp));

    // Format the message
    char message_buffer[MAX_LOG_MESSAGE_SIZE];
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);

    // Create the full log line
    char log_line[MAX_LOG_MESSAGE_SIZE + 256];
    if (function && line > 0) {
        snprintf(log_line, sizeof(log_line), "[%s] %s %s:%d - %s\n",
                 timestamp, log_level_names[level], function, line, message_buffer);
    } else {
        snprintf(log_line, sizeof(log_line), "[%s] %s - %s\n",
                 timestamp, log_level_names[level], message_buffer);
    }

    // Output to console
    if (g_log_state.console_output) {
        if (level <= LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", log_line);
        } else {
            printf("%s", log_line);
        }
        fflush(stdout);
        fflush(stderr);
    }

    // Output to file
    if (g_log_state.log_file) {
        fprintf(g_log_state.log_file, "%s", log_line);
        fflush(g_log_state.log_file);
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_log_state.log_mutex);
#else
    pthread_mutex_unlock(&g_log_state.log_mutex);
#endif
}

// Public logging functions
void log_message(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(level, NULL, 0, format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_ERROR, NULL, 0, format, args);
    va_end(args);
}

void log_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_WARNING, NULL, 0, format, args);
    va_end(args);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_INFO, NULL, 0, format, args);
    va_end(args);
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_DEBUG, NULL, 0, format, args);
    va_end(args);
}

void log_trace(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_TRACE, NULL, 0, format, args);
    va_end(args);
}

void log_with_context(log_level_t level, const char* function, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(level, function, line, format, args);
    va_end(args);
}
