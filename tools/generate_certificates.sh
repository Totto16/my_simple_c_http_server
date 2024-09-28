#!/usr/bin/env bash

set -e

PRIVATE_KEY="server.key"
TEMP_CSR_FILE="server.csr"
PUBLIC_CERT="server.crt"

openssl genrsa -out "$PRIVATE_KEY" 4096

openssl rsa -in "$PRIVATE_KEY" -out "$PRIVATE_KEY"

openssl req -sha256 -new -key "$PRIVATE_KEY" -out "$TEMP_CSR_FILE" -config ./cert.conf

openssl x509 -req -sha256 -days 20000 -in "$TEMP_CSR_FILE" -signkey "$PRIVATE_KEY" -out "$PUBLIC_CERT" --extfile v3.ext

rm "$TEMP_CSR_FILE"
