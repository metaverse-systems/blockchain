# Quickstart: Monitoring, Metrics & Health Endpoint

## Enable Monitoring

Add these fields to `config.json`:

```json
{
  "network": {
    "monitoring_enabled": true,
    "monitoring_port": 9090,
    "monitoring_bind_address": "127.0.0.1",
    "log_level": "info",
    "log_format": "text"
  }
}
```

### IPv6 Bind Example

```json
{
  "network": {
    "monitoring_enabled": true,
    "monitoring_port": 9090,
    "monitoring_bind_address": "::1"
  }
}
```

## Start the Node

```bash
./src/blockchain --data-dir node1
```

Or override the monitoring port via CLI:

```bash
./src/blockchain --data-dir node1 --monitoring-port 9100
```

## Test the Health Endpoint

The monitoring server uses TLS (same cert/key as the RPC server). Use `-k` for self-signed certs:

```bash
curl -k https://localhost:9090/health
```

Expected response:
```json
{
  "status": "healthy",
  "chain_height": 0,
  "peer_count": 0,
  "chunk_count": 1,
  "uptime_seconds": 5.2,
  "last_block_index": -1
}
```

## Test the Metrics Endpoint

```bash
curl -k https://localhost:9090/metrics
```

Expected response:
```
# HELP blockchain_chain_height Current blockchain height.
# TYPE blockchain_chain_height gauge
blockchain_chain_height 0

# HELP blockchain_peer_count Number of currently connected peers.
# TYPE blockchain_peer_count gauge
blockchain_peer_count 0

# ... (more metrics)
```

## Enable JSON Structured Logging

Set `log_format` to `"json"` in `config.json`:

```json
{
  "network": {
    "log_format": "json"
  }
}
```

Restart the node. Log output will now be JSON Lines:

```json
{"timestamp":"2026-06-23T14:30:00Z","level":"INFO","message":"Node started on port 12345"}
{"timestamp":"2026-06-23T14:30:01Z","level":"INFO","message":"Monitoring server started on 127.0.0.1:9090"}
```

## Prometheus Configuration

Add to your `prometheus.yml`. Note the `scheme: https` and TLS config:

```yaml
scrape_configs:
  - job_name: 'blockchain'
    scheme: https
    tls_config:
      insecure_skip_verify: true  # or ca_file for production certs
    static_configs:
      - targets: ['localhost:9090']
    metrics_path: '/metrics'
```

## Build & Test

```bash
# Build
make -j8

# Run monitoring unit tests
./tests/monitoring_tests

# Run monitoring integration tests
./tests/monitoring_integration_tests
```

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `curl: (7) Connection refused` | Monitoring server not enabled | Set `monitoring_enabled: true` in `config.json` |
| `curl: (60) SSL certificate` | Self-signed cert | Use `curl -k` or configure `ca_file` in Prometheus |
| Metrics show stale values | High load on node | Gauges are computed on-scrape; counters are atomic (always current) |
| JSON logs not parseable | Non-UTF-8 bytes in message | Bytes are escaped per JSON spec; use a JSON parser that handles escape sequences |
