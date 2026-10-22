#!/usr/bin/env bash
# Compile dengan sertifikat yang selalu disegarkan lebih dulu.
#   tools/build.sh [argumen tambahan arduino-cli compile]
# Contoh rilis tanpa log: tools/build.sh --build-property compiler.cpp.extra_flags=-DLOG_LEVEL=0
set -euo pipefail
cd "$(dirname "$0")/.."

FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs"

echo "== [1/3] Segarkan root CA =="
QUIET=1 bash tools/update-certs.sh refresh      # berhenti bila sumber resmi berbeda / sertifikat kedaluwarsa

echo "== [2/3] Verifikasi sertifikat tertanam =="
bash tools/verify-certs.sh src/tls.h || echo "PERINGATAN: ada sertifikat < 365 hari lagi atau tidak cocok (lihat di atas)"

echo "== [3/3] Compile =="
arduino-cli compile --clean --warnings all --fqbn "$FQBN" --export-binaries "$@" .
