#!/usr/bin/env bash
# Generate a local self-signed certificate for WSS development only.
# Never commit data/; production deployments must use a real certificate.
set -euo pipefail

out="${1:-data/tls}"
mkdir -p "$out"
openssl req -x509 -newkey rsa:2048 -nodes -days 30 \
  -keyout "$out/key.pem" -out "$out/cert.pem" \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" >/dev/null 2>&1
chmod 600 "$out/key.pem"
printf 'generated %s/cert.pem and %s/key.pem (valid 30 days)\n' "$out" "$out"
