# Quickstart: 006 — Transaction Model (Stream-Based Key/Value Store)

## Prerequisites

- Built blockchain binary (see main README)
- Running node with TLS certificates
- `openssl s_client` or any JSON-RPC client with TLS support

## Publish Data to a Stream

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"publish","params":{"stream":"assets","key":"item-42","data":"{\"name\":\"Sword\",\"damage\":50}"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

Response:
```json
{"jsonrpc":"2.0","id":"1","result":"{\"index\":1,...,\"entries\":[{\"stream\":\"assets\",\"key\":\"item-42\",\"data\":\"{\\\"name\\\":\\\"Sword\\\",\\\"damage\\\":50}\"}]}"}
```

## Auto-Create a Stream

Publishing to a non-existent stream auto-creates it:

```bash
echo '{"jsonrpc":"2.0","id":"2","method":"publish","params":{"stream":"logs","key":"event-1","data":"User logged in"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

## Explicitly Create a Stream

```bash
echo '{"jsonrpc":"2.0","id":"3","method":"createStream","params":{"name":"inventory"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

## List All Streams

```bash
echo '{"jsonrpc":"2.0","id":"4","method":"listStreams"}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

Response:
```json
{"jsonrpc":"2.0","id":"4","result":"[\"assets\",\"inventory\",\"logs\"]"}
```

## Query Entries (Full History)

```bash
echo '{"jsonrpc":"2.0","id":"5","method":"getStreamEntries","params":{"stream":"assets","key":"item-42"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

## Query Latest Entry Only

```bash
echo '{"jsonrpc":"2.0","id":"6","method":"getStreamEntry","params":{"stream":"assets","key":"item-42"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

## Query All Entries in a Stream (No Key Filter)

```bash
echo '{"jsonrpc":"2.0","id":"7","method":"getStreamEntries","params":{"stream":"assets"}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

## Configure Per-Node Stream Permissions

In `config.json`, restrict which streams accept local publishes:

```json
{
  "streams": {
    "allowed_streams": ["assets", "inventory"]
  }
}
```

Omit the `streams` section or set `"allowed_streams": []` for unrestricted access (default).

## Store Binary Data

Base64-encode on the client side before publishing:

```bash
DATA=$(base64 < myfile.bin)
echo "{\"jsonrpc\":\"2.0\",\"id\":\"9\",\"method\":\"publish\",\"params\":{\"stream\":\"files\",\"key\":\"myfile.bin\",\"data\":\"$DATA\"}}" | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

Decode after retrieval on the client side.
