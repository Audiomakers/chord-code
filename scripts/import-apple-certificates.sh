#!/bin/bash
set -euo pipefail

: "${APPLE_CERTIFICATES_P12_BASE64:?Missing APPLE_CERTIFICATES_P12_BASE64}"
: "${APPLE_CERTIFICATE_PASSWORD:?Missing APPLE_CERTIFICATE_PASSWORD}"

keychain_password="${KEYCHAIN_PASSWORD:-$(openssl rand -hex 24)}"
keychain_path="${RUNNER_TEMP:-/tmp}/chord-code-signing.keychain-db"
certificate_path="${RUNNER_TEMP:-/tmp}/chord-code-certificates.p12"

export CCP_KEYCHAIN_PATH="$keychain_path"
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "CCP_KEYCHAIN_PATH=$keychain_path" >> "$GITHUB_ENV"
fi

python3 - "$certificate_path" <<'PY'
import base64
import os
import sys

with open(sys.argv[1], "wb") as output:
    output.write(base64.b64decode(os.environ["APPLE_CERTIFICATES_P12_BASE64"]))
PY

security create-keychain -p "$keychain_password" "$keychain_path"
security set-keychain-settings -lut 21600 "$keychain_path"
security unlock-keychain -p "$keychain_password" "$keychain_path"
security import "$certificate_path" \
  -k "$keychain_path" \
  -P "$APPLE_CERTIFICATE_PASSWORD" \
  -T /usr/bin/codesign \
  -T /usr/bin/productsign
security set-key-partition-list \
  -S apple-tool:,apple:,codesign: \
  -s -k "$keychain_password" "$keychain_path"
security list-keychains -d user -s "$keychain_path" login.keychain-db

rm -f "$certificate_path"
security find-identity -v -p codesigning "$keychain_path"

