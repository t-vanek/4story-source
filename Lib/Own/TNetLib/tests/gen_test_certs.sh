#!/usr/bin/env bash
# Generate a throw-away self-signed CA + one server + one client cert
# for the tnetlib TLS test. Run by CMake at configure time; the
# generated files land under ${CMAKE_CURRENT_BINARY_DIR}/test_certs/
# and are passed to test_tnetlib_tls via macros. NOT for production —
# keys are unencrypted on disk; CN matching is trivial; lifetime is
# short.
#
# Usage: gen_test_certs.sh <output_dir>

set -euo pipefail

out="${1:-test_certs}"
mkdir -p "$out"
cd "$out"

# Idempotent: skip if files already exist (CMake re-runs configure on
# every cache touch; we don't want to regenerate on each one).
if [[ -f ca.crt && -f server.crt && -f client.crt && -f rogue_ca.crt && \
      -f rogue_client.crt && -f san_client.crt ]]; then
    exit 0
fi

# CA — self-signed, 2 years.
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout ca.key -out ca.crt \
    -days 730 \
    -subj "/CN=tnetlib-test-ca" \
    2>/dev/null

# Server — CSR + sign with CA.
openssl req -newkey rsa:2048 -nodes \
    -keyout server.key -out server.csr \
    -subj "/CN=tnetlib-test-server" \
    2>/dev/null
openssl x509 -req -in server.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt -days 365 \
    2>/dev/null

# Client — CSR + sign with CA.
openssl req -newkey rsa:2048 -nodes \
    -keyout client.key -out client.csr \
    -subj "/CN=tnetlib-test-client" \
    2>/dev/null
openssl x509 -req -in client.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out client.crt -days 365 \
    2>/dev/null

# Rogue CA — a SECOND, unrelated CA. Used to mint a "rogue" client cert
# that the trusted CA never signed; the TLS test asserts that a session
# presenting this cert is rejected during handshake.
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout rogue_ca.key -out rogue_ca.crt \
    -days 730 \
    -subj "/CN=tnetlib-rogue-ca" \
    2>/dev/null

openssl req -newkey rsa:2048 -nodes \
    -keyout rogue_client.key -out rogue_client.csr \
    -subj "/CN=tnetlib-rogue-client" \
    2>/dev/null
openssl x509 -req -in rogue_client.csr \
    -CA rogue_ca.crt -CAkey rogue_ca.key -CAcreateserial \
    -out rogue_client.crt -days 365 \
    2>/dev/null

# SAN-only client — cert with a generic CN ("tnetlib-generic") and a
# DNS-type SAN ("tnetlib-san-client") so the post-handshake identity
# check has a separate identity in CN vs SAN. Exercises the SAN
# matching path (RFC 5280 §4.2.1.6) on the server side.
cat > san_ext.cnf <<'EOF'
subjectAltName = DNS:tnetlib-san-client, DNS:tnetlib-san-alias
EOF
openssl req -newkey rsa:2048 -nodes \
    -keyout san_client.key -out san_client.csr \
    -subj "/CN=tnetlib-generic" \
    2>/dev/null
openssl x509 -req -in san_client.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out san_client.crt -days 365 \
    -extfile san_ext.cnf \
    2>/dev/null

# Drop intermediate artefacts.
rm -f server.csr client.csr rogue_client.csr san_client.csr san_ext.cnf \
      ca.srl rogue_ca.srl

chmod 600 ca.key server.key client.key rogue_ca.key rogue_client.key \
          san_client.key
