# Enclave Collector - File Structure Documentation

This document provides a comprehensive overview of every file and directory in the enclave-collector project, explaining their purpose, functionality, and relationships.

## 📁 Project Overview

The enclave-collector is a secure voting system component that processes votes using Intel SGX enclaves or simulation mode. It integrates with external APIs for data storage and election management while performing secure mathematical operations within the enclave.

## 🗂️ Directory Structure

### 📁 `/` (Root Directory)

#### Core Files
- **`CMakeLists.txt`** - Main CMake build configuration file that orchestrates the entire project build
- **`README.md`** - Project overview, setup instructions, and usage guidelines
- **`.gitignore`** - Git ignore patterns for build artifacts, logs, and temporary files

### 📁 `/common` - Shared Definitions and Types

#### Header Files (.h)
- **`constants.h`** - System-wide constants including:
  - Cryptographic sizes (AES_KEY_SIZE, RSA_KEY_SIZE, etc.)
  - Network settings (DEFAULT_PORT, MAX_CONNECTIONS, etc.)
  - Vote processing limits (MAX_VOTES_PER_BATCH, etc.)
  - Election parameters (MAX_CANDIDATES, MAX_ELECTION_ID_SIZE, etc.)
  - Storage and logging configurations

- **`error_codes.h`** - Comprehensive error code definitions:
  - General errors (INVALID_PARAMETER, OUT_OF_MEMORY, etc.)
  - Cryptographic errors (ENCRYPTION_FAILED, VERIFICATION_FAILED, etc.)
  - Vote processing errors (INVALID_VOTE, DUPLICATE_VOTE, etc.)
  - Network and storage errors
  - Function declarations: `get_error_description()`, `get_enclave_error_description()`

- **`shared_types.h`** - Core data structures shared across host and enclave:
  - `enclave_result_t` - Standard return type for all operations
  - `vote_t` - Vote data structure with encrypted content
  - `host_context_t` - Main host application state
  - `host_config_t` - Configuration parameters
  - `election_params_t` - Election setup and parameters
  - `auxiliary_values_t` - Mathematical auxiliary data for homomorphic operations
  - `final_results_t` - Election results structure

#### Source Files (.c)
- **`error_codes.c`** - Implementation of error description functions that map error codes to human-readable messages

### 📁 `/host` - Host Application (Untrusted Component)

The host application runs outside the enclave and handles I/O, networking, and coordination with external systems.

#### Core Host Files
- **`host_main.c`** - Main entry point and application lifecycle:
  - Command-line argument parsing
  - Configuration loading
  - Signal handling for graceful shutdown
  - Service initialization and main loop
  - Network server management

- **`host_interface.c/.h`** - Primary interface to enclave operations:
  - Enclave initialization and cleanup
  - Vote processing coordination
  - Simulation mode management
  - Context and state management
  - Bridge between host and enclave functions

#### Network and Communication
- **`network_interface.c/.h`** - HTTP server and network handling:
  - Socket server creation and management
  - HTTP request/response parsing
  - Client connection handling
  - Thread management for concurrent connections
  - Cross-platform networking (Windows/Linux)

- **`http_handlers.c`** - HTTP endpoint implementations:
  - `/vote` - Vote submission endpoint with external API integration
  - `/aggregate` - Vote aggregation and result calculation
  - `/info` - Enclave status and information
  - `/health` - Health check endpoint
  - Integration with external API for election parameters and results storage

#### External Integration
- **`api_client.c/.h`** - External API communication:
  - HTTP GET/POST operations using libcurl
  - Election parameter fetching (`api_get_election_parameters()`)
  - Auxiliary values retrieval (`api_get_auxiliary_values()`)
  - Vote receipt storage (`api_store_vote_receipt()`)
  - Final results submission (`api_store_final_results()`)
  - Key management (`api_get_keys()`, `api_store_keys()`)
  - Cross-platform HTTP implementation

- **`election_management.c/.h`** - Election lifecycle management:
  - Election state tracking (initialized, active, completed, finalized)
  - External API integration for election data
  - Election parameter validation and management
  - State transition handling
  - Progress tracking and persistence

#### File and Storage Operations
- **`file_operations.c/.h`** - Secure file handling:
  - File I/O with enclave_result_t return types
  - Sealed data storage and retrieval
  - File existence checking
  - Backup creation and management
  - Secure file deletion

#### Utility and Support
- **`logging.c/.h`** - Comprehensive logging system:
  - Multi-level logging (ERROR, WARNING, INFO, DEBUG, TRACE)
  - File and console output
  - Thread-safe logging with mutex protection
  - Log rotation and file management
  - Contextual logging with function names and line numbers

- **`host_callbacks.c`** - Callback functions for enclave:
  - Memory allocation callbacks
  - I/O operation callbacks
  - Networking callbacks for enclave use
  - Platform-specific implementations

### 📁 `/enclave` - Enclave Code (Trusted Component)

The enclave contains the secure, trusted code that performs cryptographic operations and vote processing.

#### Core Enclave Operations
- **`enclave_operations.h`** - Main enclave interface definitions:
  - Enclave entry points (ECALLs)
  - Data structures for secure operations
  - Function signatures for vote processing and aggregation

- **`collector_operations.c`** - Primary enclave functionality:
  - Vote collection and validation
  - Batch processing of votes
  - Integration with secure math operations
  - State management within enclave

#### Vote Processing
- **`vote_processing.c`** - Secure vote handling:
  - Vote validation and verification
  - Homomorphic encryption operations
  - Vote aggregation algorithms
  - Duplicate detection and prevention
  - Secure vote counting

#### Cryptography and Security
- **`crypto_operations.c`** - Cryptographic functions:
  - Key generation and management
  - Digital signatures (ECDSA/RSA)
  - Hash operations (SHA-256)
  - Encryption/decryption (AES-GCM)
  - Secure random number generation

- **`secure_math.c/.h`** - Secure mathematical operations:
  - Big integer arithmetic without static storage
  - Homomorphic multiplication for auxiliary values
  - Constant-time operations to prevent side-channel attacks
  - Secure memory wiping
  - Mathematical context management with external parameters

#### Storage and Persistence
- **`sealed_storage.c`** - Intel SGX sealed storage:
  - Data sealing with enclave identity
  - Secure data unsealing and verification
  - Integrity protection
  - Key derivation for sealing

#### Simulation Support
- **`enclave_sim.c`** - Simulation mode implementation:
  - Non-SGX simulation of enclave operations
  - Development and testing support
  - Debugging capabilities
  - Cross-platform compatibility

### 📁 `/config` - Configuration Files

- **`enclave.conf`** - Production configuration:
  - Network settings (port, connections)
  - Logging configuration
  - Security parameters
  - API endpoints

- **`simulation.conf`** - Simulation mode settings:
  - Development-specific configurations
  - Debug logging levels
  - Test environment parameters

### 📁 `/edl` - Enclave Definition Language

- **`collector.edl`** - Intel SGX EDL file defining:
  - Trusted functions (ECALLs) that can be called from host
  - Untrusted functions (OCALLs) that enclave can call on host
  - Data structures for marshaling between host and enclave
  - Import statements for required EDL files

### 📁 `/scripts` - Build and Deployment Scripts

- **`build_production.sh/.ps1`** - Production build scripts:
  - Release mode compilation
  - SGX production enclave signing
  - Optimization flags
  - Distribution preparation

- **`build_simulation.sh/.ps1`** - Development build scripts:
  - Debug mode compilation
  - Simulation mode enclave
  - Development tools integration
  - Testing support

### 📁 `/tests` - Testing Infrastructure

#### Test Organization
- **`CMakeLists.txt`** - Test build configuration and target definitions

#### Test Categories
- **`unit_tests/`** - Individual component tests:
  - Function-level testing
  - Mocking and stubbing
  - Isolated component validation
  - Code coverage analysis

- **`integration_tests/`** - System integration tests:
  - End-to-end workflow testing
  - Host-enclave communication
  - External API integration
  - Full system scenarios

- **`simulation_tests/`** - Simulation mode specific tests:
  - Cross-platform compatibility
  - Development environment validation
  - Performance benchmarking

- **`test_data/`** - Test fixtures and data:
  - Sample votes and election data
  - Mock API responses
  - Test certificates and keys
  - Expected results and validation data

### 📁 `/docs` - Documentation

- **`FILE_STRUCTURE.md`** - This comprehensive file structure documentation

### 📁 `/build` - Build Artifacts (Generated)

Auto-generated directory containing:
- Compiled binaries and libraries
- CMake cache and configuration files
- Intermediate build objects
- Test executables
- Package files

## 🔄 Data Flow and Integration

### External API Integration Flow
1. **Election Setup**: `election_management.c` fetches parameters via `api_client.c`
2. **Vote Submission**: `http_handlers.c` receives votes, fetches auxiliary values, processes in enclave
3. **Result Storage**: Aggregated results stored externally via `api_client.c`
4. **Key Management**: Cryptographic keys retrieved/stored through external API

### Host-Enclave Communication
1. **Initialization**: `host_interface.c` creates and initializes enclave
2. **Vote Processing**: Host passes votes to enclave via ECALLs defined in `collector.edl`
3. **Secure Operations**: Enclave performs cryptographic operations using `crypto_operations.c`
4. **Results Return**: Enclave returns processed data to host for external storage

### Security Architecture
- **Separation of Concerns**: Host handles I/O and networking, enclave handles sensitive operations
- **External Data Fetching**: Election parameters and auxiliary values fetched from external APIs
- **No Static Storage**: Enclave avoids static data, fetches parameters dynamically
- **Secure Communication**: All host-enclave communication through defined EDL interface

## 🛠️ Build System

The project uses CMake for cross-platform building:
- **Root CMakeLists.txt**: Orchestrates entire build
- **Subdirectory CMakeLists**: Handle specific component builds
- **Dependencies**: Manages external libraries (libcurl, SGX SDK)
- **Configuration**: Supports both production and simulation builds

## 🔧 Development vs Production

### Simulation Mode
- Uses `enclave_sim.c` for non-SGX environments
- Enabled via configuration files
- Supports development and testing
- Cross-platform compatibility

### Production Mode
- Uses real Intel SGX enclaves
- Hardware security features
- Signed enclaves for deployment
- Performance optimizations

This architecture ensures a secure, scalable, and maintainable voting system that integrates with external services while maintaining the security properties of SGX enclaves.
