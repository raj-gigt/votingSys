# Environment Configuration

This document describes the environment variables that can be used to configure the enclave-collector system.

## API Configuration

### ENCLAVE_API_BASE_URL
- **Description**: Base URL for the external voting system API
- **Default**: `http://localhost:3000`
- **Examples**: 
  - Development: `http://localhost:3000`
  - Production: `https://voting-api.your-domain.com`
  - Docker: `http://voting-backend:3000`

### ENCLAVE_API_AUTH_TOKEN
- **Description**: Bearer token for API authentication
- **Default**: Empty (no authentication)
- **Example**: `eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...`

### ENCLAVE_API_TIMEOUT_MS
- **Description**: HTTP request timeout in milliseconds
- **Default**: `30000` (30 seconds)
- **Range**: 1000 - 300000 (1 second to 5 minutes)

### ENCLAVE_API_MAX_RETRIES
- **Description**: Maximum number of retry attempts for failed API requests
- **Default**: `3`
- **Range**: 0 - 10

## Logging Configuration

### ENCLAVE_LOG_LEVEL
- **Description**: Logging verbosity level
- **Default**: `INFO`
- **Values**: `DEBUG`, `INFO`, `WARNING`, `ERROR`

## Data Storage Configuration

### ENCLAVE_DATA_DIR
- **Description**: Directory for storing enclave data files
- **Default**: `./data`
- **Examples**:
  - Development: `./data`
  - Production: `/var/lib/enclave-collector`
  - Docker: `/app/data`

## Security Configuration

### ENCLAVE_ENABLE_TLS
- **Description**: Enable TLS/SSL for API communications
- **Default**: `true`
- **Values**: `true`, `false`, `1`, `0`

### ENCLAVE_ENABLE_AUTH
- **Description**: Enable authentication for API requests
- **Default**: `true`
- **Values**: `true`, `false`, `1`, `0`

## Configuration Priority

The system loads configuration in the following order (highest to lowest priority):

1. **Environment Variables** (highest priority)
2. **Configuration File** (`config/enclave_config.json`)
3. **Default Values** (lowest priority)

## Examples

### Development Environment
```bash
export ENCLAVE_API_BASE_URL="http://localhost:3000"
export ENCLAVE_LOG_LEVEL="DEBUG"
export ENCLAVE_DATA_DIR="./dev-data"
export ENCLAVE_ENABLE_AUTH="false"
```

### Production Environment
```bash
export ENCLAVE_API_BASE_URL="https://voting-api.production.com"
export ENCLAVE_API_AUTH_TOKEN="your-production-token-here"
export ENCLAVE_API_TIMEOUT_MS="60000"
export ENCLAVE_LOG_LEVEL="WARNING"
export ENCLAVE_DATA_DIR="/var/lib/enclave-collector"
export ENCLAVE_ENABLE_TLS="true"
export ENCLAVE_ENABLE_AUTH="true"
```

### Docker Environment
```yaml
# docker-compose.yml
environment:
  - ENCLAVE_API_BASE_URL=http://voting-backend:3000
  - ENCLAVE_API_AUTH_TOKEN=${API_TOKEN}
  - ENCLAVE_LOG_LEVEL=INFO
  - ENCLAVE_DATA_DIR=/app/data
```

### PowerShell (Windows)
```powershell
$env:ENCLAVE_API_BASE_URL = "http://localhost:3000"
$env:ENCLAVE_LOG_LEVEL = "DEBUG"
$env:ENCLAVE_DATA_DIR = ".\dev-data"
```

## Configuration File Format

The system can also load configuration from JSON files:

```json
{
  "api_base_url": "http://localhost:3000",
  "api_auth_token": "",
  "api_timeout_ms": 30000,
  "api_max_retries": 3,
  "log_level": "INFO",
  "data_directory": "./data",
  "enable_tls": true,
  "enable_auth": false
}
```

## Deployment Considerations

### Kubernetes
Use ConfigMaps and Secrets for environment variables:

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: enclave-config
data:
  ENCLAVE_API_BASE_URL: "https://voting-api.cluster.local"
  ENCLAVE_LOG_LEVEL: "INFO"
  ENCLAVE_DATA_DIR: "/app/data"
---
apiVersion: v1
kind: Secret
metadata:
  name: enclave-secrets
data:
  ENCLAVE_API_AUTH_TOKEN: <base64-encoded-token>
```

### Docker Swarm
Use Docker secrets for sensitive configuration:

```bash
echo "your-api-token" | docker secret create api_token -
```

### Systemd Service
```ini
[Unit]
Description=Enclave Collector Service

[Service]
Environment=ENCLAVE_API_BASE_URL=https://voting-api.production.com
Environment=ENCLAVE_LOG_LEVEL=WARNING
EnvironmentFile=/etc/enclave-collector/environment
ExecStart=/usr/local/bin/collector_host

[Install]
WantedBy=multi-user.target
```
