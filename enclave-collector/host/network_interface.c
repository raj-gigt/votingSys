#include "network_interface.h"
#include "host_interface.h"
#include "logging.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <pthread.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define SOCKET_ERROR_CODE errno
#endif

// Global server instance for thread communication
static network_server_t* g_server_instance = NULL;

// Thread function for handling client connections
static void* server_thread_func(void* arg);
static enclave_result_t handle_client_connection(int client_socket);
static enclave_result_t route_http_request(const http_request_t* request, http_response_t* response);

// Initialize network subsystem
enclave_result_t network_initialize(const network_config_t* config, network_server_t* server) {
    if (!config || !server) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    memset(server, 0, sizeof(network_server_t));
    memcpy(&server->config, config, sizeof(network_config_t));
    server->server_socket = INVALID_SOCKET;
    server->is_running = false;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_error("WSAStartup failed: %d", result);
        return ENCLAVE_ERROR_NETWORK_INIT;
    }
#endif

    log_info("Network subsystem initialized");
    return ENCLAVE_SUCCESS;
}

// Start the network server
enclave_result_t network_start_server(network_server_t* server) {
    if (!server) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (server->is_running) {
        return ENCLAVE_ERROR_ALREADY_RUNNING;
    }

    // Create socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket == INVALID_SOCKET) {
        log_error("Failed to create socket: %d", SOCKET_ERROR_CODE);
        return ENCLAVE_ERROR_NETWORK_SOCKET;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        log_warning("Failed to set SO_REUSEADDR: %d", SOCKET_ERROR_CODE);
    }

    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server->config.port);

    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        log_error("Failed to bind socket to port %d: %d", server->config.port, SOCKET_ERROR_CODE);
        close(server->server_socket);
        return ENCLAVE_ERROR_NETWORK_BIND;
    }

    // Listen for connections
    if (listen(server->server_socket, server->config.max_connections) == SOCKET_ERROR) {
        log_error("Failed to listen on socket: %d", SOCKET_ERROR_CODE);
        close(server->server_socket);
        return ENCLAVE_ERROR_NETWORK_LISTEN;
    }

    // Start server thread
    server->is_running = true;
    g_server_instance = server;

#ifdef _WIN32
    server->server_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_thread_func, server, 0, NULL);
    if (server->server_thread == NULL) {
        log_error("Failed to create server thread");
        server->is_running = false;
        close(server->server_socket);
        return ENCLAVE_ERROR_THREAD_CREATE;
    }
#else
    if (pthread_create(&server->server_thread, NULL, server_thread_func, server) != 0) {
        log_error("Failed to create server thread");
        server->is_running = false;
        close(server->server_socket);
        return ENCLAVE_ERROR_THREAD_CREATE;
    }
#endif

    log_info("Network server started on %s:%d", server->config.host, server->config.port);
    return ENCLAVE_SUCCESS;
}

// Stop the network server
enclave_result_t network_stop_server(network_server_t* server) {
    if (!server) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    if (!server->is_running) {
        return ENCLAVE_SUCCESS;
    }

    server->is_running = false;

    // Close server socket to break accept loop
    if (server->server_socket != INVALID_SOCKET) {
        close(server->server_socket);
        server->server_socket = INVALID_SOCKET;
    }

    // Wait for server thread to finish
#ifdef _WIN32
    if (server->server_thread != NULL) {
        WaitForSingleObject(server->server_thread, 5000); // 5 second timeout
        CloseHandle(server->server_thread);
        server->server_thread = NULL;
    }
#else
    if (server->server_thread != 0) {
        pthread_join(server->server_thread, NULL);
        server->server_thread = 0;
    }
#endif

    log_info("Network server stopped");
    return ENCLAVE_SUCCESS;
}

// Cleanup network resources
enclave_result_t network_cleanup(network_server_t* server) {
    if (!server) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    network_stop_server(server);

#ifdef _WIN32
    WSACleanup();
#endif

    log_info("Network subsystem cleaned up");
    return ENCLAVE_SUCCESS;
}

// Server thread function
static void* server_thread_func(void* arg) {
    network_server_t* server = (network_server_t*)arg;
    
    log_info("Server thread started, listening for connections...");

    while (server->is_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server->server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket == INVALID_SOCKET) {
            if (server->is_running) {
                log_error("Failed to accept connection: %d", SOCKET_ERROR_CODE);
            }
            break;
        }

        log_debug("Accepted connection from client");
        
        // Handle client in same thread (for simplicity)
        // In production, you might want to use a thread pool
        enclave_result_t result = handle_client_connection(client_socket);
        if (result != ENCLAVE_SUCCESS) {
            log_warning("Failed to handle client connection: %s", get_error_description(result));
        }
        
        close(client_socket);
    }

    log_info("Server thread terminated");
    return NULL;
}

// Handle individual client connection
static enclave_result_t handle_client_connection(int client_socket) {
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        log_warning("Failed to receive data from client: %d", SOCKET_ERROR_CODE);
        return ENCLAVE_ERROR_NETWORK_RECEIVE;
    }
    
    buffer[bytes_received] = '\0';
    log_debug("Received HTTP request (%d bytes)", bytes_received);

    // Parse HTTP request
    http_request_t request;
    enclave_result_t result = parse_http_request(buffer, &request);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to parse HTTP request: %s", get_error_description(result));
        const char* error_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, error_response, strlen(error_response), 0);
        return result;
    }

    // Route and handle request
    http_response_t response;
    memset(&response, 0, sizeof(response));
    
    result = route_http_request(&request, &response);
    if (result != ENCLAVE_SUCCESS) {
        log_warning("Failed to handle HTTP request: %s", get_error_description(result));
        response.status_code = 500;
        response.body = strdup("Internal Server Error");
        response.body_length = strlen(response.body);
    }

    // Build and send response
    char* raw_response;
    result = build_http_response(&response, &raw_response);
    if (result == ENCLAVE_SUCCESS) {
        send(client_socket, raw_response, strlen(raw_response), 0);
        free(raw_response);
    }

    // Cleanup
    free_http_request(&request);
    free_http_response(&response);

    return ENCLAVE_SUCCESS;
}

// Route HTTP request to appropriate handler
static enclave_result_t route_http_request(const http_request_t* request, http_response_t* response) {
    if (!request || !response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    log_debug("Routing %s %s", request->method, request->path);

    if (strcmp(request->path, "/health") == 0) {
        return handle_health_check(request, response);
    } else if (strcmp(request->path, "/vote") == 0 && strcmp(request->method, "POST") == 0) {
        return handle_vote_submission(request, response);
    } else if (strcmp(request->path, "/aggregation") == 0 && strcmp(request->method, "GET") == 0) {
        return handle_vote_aggregation(request, response);
    } else if (strcmp(request->path, "/info") == 0 && strcmp(request->method, "GET") == 0) {
        return handle_enclave_info(request, response);
    } else {
        // 404 Not Found
        response->status_code = 404;
        response->body = strdup("Not Found");
        response->body_length = strlen(response->body);
        return ENCLAVE_SUCCESS;
    }
}

// Parse HTTP request
enclave_result_t parse_http_request(const char* raw_request, http_request_t* request) {
    if (!raw_request || !request) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    memset(request, 0, sizeof(http_request_t));

    // Parse request line (METHOD PATH HTTP/1.1)
    const char* line_end = strstr(raw_request, "\r\n");
    if (!line_end) {
        return ENCLAVE_ERROR_INVALID_HTTP_REQUEST;
    }

    // Extract method and path
    sscanf(raw_request, "%15s %255s", request->method, request->path);

    // Find body (after \r\n\r\n)
    const char* body_start = strstr(raw_request, "\r\n\r\n");
    if (body_start) {
        body_start += 4; // Skip \r\n\r\n
        request->body_length = strlen(body_start);
        if (request->body_length > 0) {
            request->body = malloc(request->body_length + 1);
            if (request->body) {
                memcpy(request->body, body_start, request->body_length);
                request->body[request->body_length] = '\0';
            }
        }
    }

    return ENCLAVE_SUCCESS;
}

// Build HTTP response
enclave_result_t build_http_response(const http_response_t* response, char** raw_response) {
    if (!response || !raw_response) {
        return ENCLAVE_ERROR_INVALID_PARAMETER;
    }

    const char* status_text;
    switch (response->status_code) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }

    size_t content_length = response->body_length;
    size_t response_size = 512 + content_length; // Headers + body

    *raw_response = malloc(response_size);
    if (!*raw_response) {
        return ENCLAVE_ERROR_MEMORY_ALLOCATION;
    }

    snprintf(*raw_response, response_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        response->status_code, status_text, content_length,
        response->body ? response->body : "");

    return ENCLAVE_SUCCESS;
}

// Free HTTP request resources
void free_http_request(http_request_t* request) {
    if (request && request->body) {
        free(request->body);
        request->body = NULL;
        request->body_length = 0;
    }
}

// Free HTTP response resources
void free_http_response(http_response_t* response) {
    if (response && response->body) {
        free(response->body);
        response->body = NULL;
        response->body_length = 0;
    }
}
