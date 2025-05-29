# Enclave Collector

A secure voting system collector component designed to run in Intel SGX enclaves with WSL simulation support for development.

## Overview

The Enclave Collector is responsible for:
- Securely processing and validating votes
- Performing cryptographic operations in a trusted environment
- Aggregating vote results with privacy preservation
- Providing attestation capabilities for verification

## Architecture

```
enclave-collector/
├── host/           # Untrusted host application
├── enclave/        # Trusted enclave code (SGX)
├── common/         # Shared data structures and definitions
├── edl/            # Enclave Definition Language files
├── config/         # Configuration files
├── tests/          # Test suite
├── scripts/        # Build and deployment scripts
└── docs/           # Documentation
```

## Features

### Core Functionality
- **Vote Processing**: Secure validation and processing of encrypted votes
- **Cryptographic Operations**: Key generation, encryption, digital signatures
- **Vote Aggregation**: Privacy-preserving vote counting and result generation
- **Secure Storage**: Sealed data storage for persistent state

### Security Features
- **Intel SGX Support**: Hardware-based trusted execution environment
- **Remote Attestation**: Cryptographic proof of enclave integrity
- **Simulation Mode**: WSL/development environment support
- **Secure Communication**: Encrypted host-enclave communication

### Development Features
- **WSL Compatibility**: Full simulation mode for Windows development
- **Comprehensive Testing**: Unit, integration, and end-to-end tests
- **Flexible Configuration**: Environment-specific settings
- **Detailed Logging**: Multi-level logging with file and console output

## Quick Start

### Prerequisites

#### For Simulation Mode (WSL/Development)
- CMake 3.16+
- GCC/Clang or Visual Studio 2019+
- Git with submodules support

#### For Production (SGX Hardware)
- Intel SGX SDK
- Open Enclave SDK
- SGX-enabled hardware

### Building

#### Simulation Mode (Recommended for Development)
```bash
# Clone with dependencies
git clone --recursive <repository-url>
cd enclave-collector

# Build in simulation mode
./scripts/build_simulation.sh

# Or with PowerShell on Windows
./scripts/build_simulation.ps1
```

#### Production Mode
```bash
# Build for SGX hardware
./scripts/build_production.sh
```

### Running

#### Start the Collector Host
```bash
# Default configuration
./build/bin/collector_host

# Custom configuration
./build/bin/collector_host -c config/enclave.conf -p 8080

# With verbose logging
./build/bin/collector_host -l 4 -s
```

#### Command Line Options
- `-p, --port <port>`: Set listening port (default: 8080)
- `-c, --config <file>`: Configuration file path
- `-l, --log-level <level>`: Log level (0-4)
- `-s, --simulation`: Force simulation mode
- `-h, --help`: Show help message
- `-v, --version`: Show version information

## Configuration

### Main Configuration (`config/enclave.conf`)
```ini
# Network settings
port=8080
max_connections=100

# Security settings
simulation_mode=true
enable_attestation=false

# Logging settings
log_level=2
log_file=logs/collector.log
```

### Simulation Configuration (`config/simulation.conf`)
```ini
# WSL specific settings
wsl_mode=true
debug_mode=true
mock_attestation=true
```

## API Interface

The collector exposes a network interface for vote processing:

### Vote Submission
```json
POST /vote
{
  "voter_id": "voter123",
  "candidate_id": 1,
  "encrypted_vote": "...",
  "signature": "..."
}
```

### Vote Aggregation
```json
GET /aggregate
{
  "results": [
    {"candidate_id": 1, "vote_count": 150},
    {"candidate_id": 2, "vote_count": 200}
  ],
  "total_votes": 350,
  "proof": "..."
}
```

### System Status
```json
GET /status
{
  "total_votes": 350,
  "valid_votes": 345,
  "invalid_votes": 5,
  "is_initialized": true
}
```

## Development

### Building Tests
```bash
# Build with tests
./scripts/build_simulation.sh test

# Run specific test suite
cd build && ctest -R "unit_tests"
```

### Adding New Features

1. **Define Interface**: Update `edl/collector.edl` with new ECALLs/OCALLs
2. **Implement Enclave**: Add trusted functions in `enclave/`
3. **Implement Host**: Add untrusted functions in `host/`
4. **Add Tests**: Create tests in `tests/`
5. **Update Documentation**: Update this README and API docs

### Simulation vs Production

| Feature | Simulation Mode | Production Mode |
|---------|----------------|-----------------|
| Crypto | Software implementation | Hardware-accelerated |
| Attestation | Mock/disabled | Remote attestation |
| Sealed Storage | File-based | Hardware-sealed |
| Performance | Development speed | Production security |

## Testing

### Test Categories

#### Unit Tests (`tests/unit_tests/`)
- Individual function testing
- Cryptographic operation validation
- Data structure verification

#### Integration Tests (`tests/integration_tests/`)
- Host-enclave communication
- End-to-end vote processing
- Configuration validation

#### Simulation Tests (`tests/simulation_tests/`)
- WSL-specific functionality
- Mock service validation
- Development environment testing

### Running Tests
```bash
# All tests
ctest

# Specific category
ctest -R "unit_tests"

# Verbose output
ctest --output-on-failure
```

## Dependencies

### External Dependencies
- **libtommath**: Big integer arithmetic
- **libcurl**: Network communication
- **Open Enclave SDK**: SGX enclave support (production)

### Dependency Management
Dependencies are managed via Git submodules and CMake:
```bash
# Initialize submodules
git submodule update --init --recursive

# Update dependencies
git submodule update --remote
```

## Troubleshooting

### Common Issues

#### WSL Build Issues
```bash
# Update WSL
wsl --update

# Install build tools
sudo apt update && sudo apt install build-essential cmake
```

#### Dependency Issues
```bash
# Reinitialize submodules
git submodule deinit --all
git submodule update --init --recursive
```

#### SGX Issues (Production)
```bash
# Check SGX support
sgx_cap

# Install SGX driver
sudo apt install sgx-driver
```

### Debug Mode
```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Run with debugger
gdb ./build/bin/collector_host
```

## Security Considerations

### Simulation Mode Security
- **NOT FOR PRODUCTION**: Simulation mode provides no security guarantees
- **Development Only**: Use only for testing and development
- **Mock Crypto**: Cryptographic operations are simulated

### Production Security
- **SGX Required**: Production requires SGX-enabled hardware
- **Attestation**: Remote attestation verifies enclave integrity
- **Sealed Storage**: Hardware-based sealed data protection

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

[License information]

## Support

For issues and questions:
- Check the troubleshooting section
- Review test cases for examples
- Create an issue with detailed information

## Implementation Status

### ✅ Completed Components

#### Core Infrastructure
- **Project Structure**: Complete directory hierarchy with proper separation of concerns
- **Build System**: CMake-based build with dependency integration and cross-platform support
- **Configuration Management**: Flexible configuration system for simulation and production modes
- **Error Handling**: Centralized error code system with descriptive messages

#### Host Application
- **Main Application**: Command-line interface with argument parsing and configuration loading
- **Network Interface**: HTTP server for vote submission and result retrieval
- **File Operations**: Secure file I/O with backup and recovery functionality
- **Logging System**: Thread-safe logging with configurable levels and output targets
- **OCALL Implementation**: Complete callback system for enclave-to-host communication

#### Enclave Implementation
- **Core Operations**: Vote processing, validation, and aggregation logic
- **Cryptographic Functions**: Key generation, signing, and verification (simulation mode)
- **Sealed Storage**: Data sealing and unsealing for persistent state
- **Vote Processing**: Batch processing and duplicate detection
- **Simulation Mode**: Complete enclave simulation for development environment

#### Network API
- **HTTP Endpoints**:
  - `GET /health` - Health check endpoint
  - `POST /vote` - Vote submission endpoint
  - `GET /aggregation` - Vote aggregation results
  - `GET /info` - Enclave information and statistics
- **Request/Response Handling**: JSON parsing and response generation
- **Error Handling**: Proper HTTP status codes and error messages

#### Testing Framework
- **Unit Tests**: Individual component testing with custom test framework
- **Integration Tests**: End-to-end workflow testing
- **Test Coverage**: Vote processing, crypto operations, network interface, file operations
- **Simulation Testing**: Complete test suite running in simulation mode

#### Build and Deployment
- **Simulation Builds**: Complete WSL/Linux and Windows PowerShell build scripts
- **Production Builds**: SGX hardware build scripts for deployment
- **Dependency Management**: Integration with existing deps folder structure
- **Cross-Platform**: Support for Windows, Linux, and WSL environments

### 🚧 Remaining Work

#### Production SGX Features
- **Hardware Attestation**: Real SGX attestation implementation (currently simulated)
- **Production Crypto**: Integration with SGX cryptographic APIs (currently uses simulation)
- **Sealed Storage**: Hardware-based sealed storage (currently uses memory simulation)
- **Platform Services**: Integration with Intel SGX platform services

#### Enhanced Security
- **Input Validation**: Extended validation for production security requirements
- **Rate Limiting**: Network endpoint rate limiting and DDoS protection
- **SSL/TLS**: HTTPS support for production deployment
- **Access Control**: Authentication and authorization mechanisms

#### Production Features
- **Database Integration**: Persistent storage backend for vote data
- **Monitoring**: Performance monitoring and health metrics
- **Load Balancing**: Multi-instance deployment support
- **Configuration Management**: Production-grade configuration and secrets management

## Implementation Details

### Network API Usage

#### Submit a Vote
```bash
curl -X POST http://localhost:8080/vote \
  -H "Content-Type: application/json" \
  -d '{"candidate_id": 1, "vote_id": "voter123", "timestamp": 1640995200}'
```

#### Get Vote Aggregation
```bash
curl http://localhost:8080/aggregation
```

#### Check Enclave Status
```bash
curl http://localhost:8080/info
```

### Build Instructions

#### Simulation Mode (Development)
```bash
# Linux/WSL
./scripts/build_simulation.sh

# Windows PowerShell
./scripts/build_simulation.ps1
```

#### Production Mode (SGX Hardware)
```bash
# Linux
./scripts/build_production.sh

# Windows
./scripts/build_production.ps1
```

### Testing
```bash
# Run unit tests
cd build-simulation
make run_unit_tests

# Run integration tests
./bin/integration_test
```

### Running the Service
```bash
# Start collector host
./bin/collector_host --port 8080 --simulation

# With custom configuration
./bin/collector_host --config config/enclave.conf --log-level 3
```
