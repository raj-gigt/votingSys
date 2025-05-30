# Deployment Guide

This guide explains how to deploy the enclave-collector system with configurable API endpoints for different environments.

## Quick Start

### 1. Local Development
```bash
# Set environment variables
export ENCLAVE_API_BASE_URL="http://localhost:3000"
export ENCLAVE_LOG_LEVEL="DEBUG"

# Build and run
cd enclave-collector
mkdir -p build && cd build
cmake ..
make
./host/collector_host
```

### 2. Production Deployment
```bash
# Set production environment variables
export ENCLAVE_API_BASE_URL="https://your-production-api.com"
export ENCLAVE_API_AUTH_TOKEN="your-production-token"
export ENCLAVE_LOG_LEVEL="WARNING"
export ENCLAVE_DATA_DIR="/var/lib/enclave-collector"

# Use production config file
./host/collector_host --config ./config/production_config.json
```

## Configuration Methods

### Method 1: Environment Variables (Recommended for Production)

**Advantages:**
- Easy to manage in containerized environments
- Secure handling of sensitive data
- No config files to manage

**Setup:**
```bash
# Required
export ENCLAVE_API_BASE_URL="https://your-api-endpoint.com"

# Optional
export ENCLAVE_API_AUTH_TOKEN="your-bearer-token"
export ENCLAVE_API_TIMEOUT_MS="60000"
export ENCLAVE_LOG_LEVEL="INFO"
```

### Method 2: Configuration File

**Advantages:**
- Easy to version control
- Good for development environments
- Complex configurations

**Setup:**
1. Create config file:
```json
{
  "api_base_url": "https://your-api-endpoint.com",
  "api_auth_token": "",
  "api_timeout_ms": 30000,
  "log_level": "INFO"
}
```

2. Run with config:
```bash
./collector_host --config /path/to/config.json
```

### Method 3: Mixed Configuration

Environment variables override config file settings:

```bash
# Set base config in file
export CONFIG_FILE="./config/staging_config.json"

# Override specific settings
export ENCLAVE_API_BASE_URL="https://staging-api.com"
export ENCLAVE_LOG_LEVEL="DEBUG"

./collector_host --config $CONFIG_FILE
```

## Environment-Specific Configurations

### Development Environment

**File:** `config/development_config.json`
```json
{
  "api_base_url": "http://localhost:3000",
  "api_auth_token": "",
  "api_timeout_ms": 30000,
  "api_max_retries": 3,
  "log_level": "DEBUG",
  "data_directory": "./dev-data",
  "enable_tls": false,
  "enable_auth": false
}
```

**Environment:**
```bash
export ENCLAVE_API_BASE_URL="http://localhost:3000"
export ENCLAVE_LOG_LEVEL="DEBUG"
export ENCLAVE_ENABLE_AUTH="false"
```

### Staging Environment

**File:** `config/staging_config.json`
```json
{
  "api_base_url": "https://staging-api.your-domain.com",
  "api_auth_token": "${ENCLAVE_API_AUTH_TOKEN}",
  "api_timeout_ms": 45000,
  "api_max_retries": 5,
  "log_level": "INFO",
  "data_directory": "/var/lib/enclave-collector-staging",
  "enable_tls": true,
  "enable_auth": true
}
```

**Environment:**
```bash
export ENCLAVE_API_AUTH_TOKEN="staging-bearer-token"
export ENCLAVE_LOG_LEVEL="INFO"
```

### Production Environment

**File:** `config/production_config.json`
```json
{
  "api_base_url": "https://api.your-production-domain.com",
  "api_auth_token": "${ENCLAVE_API_AUTH_TOKEN}",
  "api_timeout_ms": 60000,
  "api_max_retries": 5,
  "log_level": "WARNING",
  "data_directory": "/var/lib/enclave-collector",
  "enable_tls": true,
  "enable_auth": true
}
```

**Environment:**
```bash
export ENCLAVE_API_AUTH_TOKEN="production-bearer-token"
export ENCLAVE_DATA_DIR="/var/lib/enclave-collector"
```

## Container Deployments

### Docker

**Dockerfile:**
```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy application
COPY build/host/collector_host /usr/local/bin/
COPY config/ /app/config/

# Create data directory
RUN mkdir -p /app/data

# Set working directory
WORKDIR /app

# Default configuration
ENV ENCLAVE_API_BASE_URL="http://voting-backend:3000"
ENV ENCLAVE_LOG_LEVEL="INFO"
ENV ENCLAVE_DATA_DIR="/app/data"

CMD ["/usr/local/bin/collector_host"]
```

**Docker Compose:**
```yaml
version: '3.8'
services:
  enclave-collector:
    build: .
    environment:
      - ENCLAVE_API_BASE_URL=http://voting-backend:3000
      - ENCLAVE_API_AUTH_TOKEN=${API_TOKEN}
      - ENCLAVE_LOG_LEVEL=INFO
    volumes:
      - enclave_data:/app/data
    depends_on:
      - voting-backend

  voting-backend:
    image: your-voting-backend:latest
    ports:
      - "3000:3000"

volumes:
  enclave_data:
```

### Kubernetes

**ConfigMap:**
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: enclave-collector-config
data:
  ENCLAVE_API_BASE_URL: "https://voting-api.cluster.local"
  ENCLAVE_LOG_LEVEL: "INFO"
  ENCLAVE_DATA_DIR: "/app/data"
  config.json: |
    {
      "api_base_url": "https://voting-api.cluster.local",
      "api_timeout_ms": 60000,
      "log_level": "INFO",
      "data_directory": "/app/data",
      "enable_tls": true,
      "enable_auth": true
    }
```

**Secret:**
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: enclave-collector-secrets
type: Opaque
data:
  ENCLAVE_API_AUTH_TOKEN: <base64-encoded-token>
```

**Deployment:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: enclave-collector
spec:
  replicas: 1
  selector:
    matchLabels:
      app: enclave-collector
  template:
    metadata:
      labels:
        app: enclave-collector
    spec:
      containers:
      - name: collector
        image: enclave-collector:latest
        envFrom:
        - configMapRef:
            name: enclave-collector-config
        - secretRef:
            name: enclave-collector-secrets
        volumeMounts:
        - name: config-volume
          mountPath: /app/config
        - name: data-volume
          mountPath: /app/data
      volumes:
      - name: config-volume
        configMap:
          name: enclave-collector-config
      - name: data-volume
        persistentVolumeClaim:
          claimName: enclave-data-pvc
```

## Security Considerations

### 1. API Token Management

**Development:**
- Use empty token or development-only token
- Disable authentication if possible

**Production:**
- Use strong, rotating bearer tokens
- Store tokens in secure secret management systems
- Never commit tokens to version control

### 2. TLS/SSL Configuration

**Development:**
```bash
export ENCLAVE_ENABLE_TLS="false"  # HTTP only
```

**Production:**
```bash
export ENCLAVE_ENABLE_TLS="true"   # HTTPS required
export ENCLAVE_API_BASE_URL="https://api.production.com"
```

### 3. Network Security

- Use internal networks for service-to-service communication
- Implement proper firewall rules
- Consider using service mesh for advanced networking

## Monitoring and Logging

### Log Level Configuration

**Development:**
```bash
export ENCLAVE_LOG_LEVEL="DEBUG"  # Verbose logging
```

**Production:**
```bash
export ENCLAVE_LOG_LEVEL="WARNING"  # Minimal logging
```

### Health Checks

The collector provides health check endpoints:
- `/health` - Basic health status
- `/metrics` - Performance metrics

Configure your deployment platform to use these endpoints.

## Troubleshooting

### Common Issues

1. **Connection Refused:**
   ```
   Error: Failed to connect to API endpoint
   Solution: Check ENCLAVE_API_BASE_URL and network connectivity
   ```

2. **Authentication Failed:**
   ```
   Error: HTTP 401 Unauthorized
   Solution: Verify ENCLAVE_API_AUTH_TOKEN is correct
   ```

3. **Timeout Errors:**
   ```
   Error: Request timeout
   Solution: Increase ENCLAVE_API_TIMEOUT_MS or check API performance
   ```

### Debug Mode

Enable debug logging to troubleshoot configuration issues:

```bash
export ENCLAVE_LOG_LEVEL="DEBUG"
./collector_host
```

This will show:
- Configuration loading process
- API endpoint resolution
- HTTP request/response details
- Authentication token validation

## Migration from Static Configuration

If migrating from a version with hardcoded API endpoints:

1. **Identify Current Endpoints:**
   ```bash
   grep -r "localhost:3000" ./src/
   ```

2. **Set Environment Variables:**
   ```bash
   export ENCLAVE_API_BASE_URL="your-current-endpoint"
   ```

3. **Test Configuration:**
   ```bash
   ./collector_host --dry-run  # If supported
   ```

4. **Deploy Incrementally:**
   - Start with development environment
   - Move to staging
   - Finally update production

## Best Practices

1. **Environment Separation:**
   - Use different API endpoints for each environment
   - Implement proper secret management
   - Use configuration validation

2. **Security:**
   - Rotate API tokens regularly
   - Use HTTPS in production
   - Implement proper access controls

3. **Monitoring:**
   - Monitor API connectivity
   - Track configuration changes
   - Alert on authentication failures

4. **Documentation:**
   - Document all configuration options
   - Maintain environment-specific runbooks
   - Keep deployment guides updated
