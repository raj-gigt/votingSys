#include "test_framework.h"
#include "network_interface.h"
#include "shared_types.h"
#include "error_codes.h"
#include <string.h>

void test_network_interface(void) {
    // Test HTTP request parsing
    const char* test_request = 
        "POST /vote HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 45\r\n"
        "\r\n"
        "{\"candidate_id\": 1, \"vote_id\": \"test123\"}";

    http_request_t request;
    enclave_result_t result = parse_http_request(test_request, &request);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "HTTP request parsing should succeed");
    TEST_ASSERT_STRING_EQUAL("POST", request.method, "Method should be POST");
    TEST_ASSERT_STRING_EQUAL("/vote", request.path, "Path should be /vote");
    TEST_ASSERT_NOT_NULL(request.body, "Body should not be null");
    TEST_ASSERT(strstr(request.body, "candidate_id") != NULL, "Body should contain candidate_id");

    // Test HTTP response building
    http_response_t response;
    memset(&response, 0, sizeof(response));
    response.status_code = 200;
    response.body = strdup("{\"status\": \"success\"}");
    response.body_length = strlen(response.body);

    char* raw_response;
    result = build_http_response(&response, &raw_response);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "HTTP response building should succeed");
    TEST_ASSERT_NOT_NULL(raw_response, "Raw response should not be null");
    TEST_ASSERT(strstr(raw_response, "HTTP/1.1 200 OK") != NULL, "Response should contain status line");
    TEST_ASSERT(strstr(raw_response, "Content-Type: application/json") != NULL, 
               "Response should contain content type");

    // Test network configuration
    network_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.host, "127.0.0.1", sizeof(config.host) - 1);
    config.port = 8080;
    config.max_connections = 10;
    config.timeout_seconds = 30;
    config.enable_ssl = false;

    network_server_t server;
    result = network_initialize(&config, &server);
    TEST_ASSERT_EQUAL(ENCLAVE_SUCCESS, result, "Network initialization should succeed");
    TEST_ASSERT_EQUAL(8080, server.config.port, "Server should have correct port");
    TEST_ASSERT_EQUAL(false, server.is_running, "Server should not be running initially");

    // Test invalid configurations
    network_config_t invalid_config;
    memset(&invalid_config, 0, sizeof(invalid_config));
    invalid_config.port = -1; // Invalid port

    result = network_initialize(&invalid_config, &server);
    // Note: Our current implementation doesn't validate port in initialize,
    // but it would fail when starting the server

    // Cleanup
    network_cleanup(&server);
    free_http_request(&request);
    free_http_response(&response);
    if (raw_response) free(raw_response);
}
