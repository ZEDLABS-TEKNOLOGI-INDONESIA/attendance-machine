#!/usr/bin/env bash
set -euo pipefail
INO=attendance-machine.ino
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

# Pecah blok TLS_ROOT_CA di .ino menjadi satu file per sertifikat
awk '/TLS_ROOT_CA\[\] PROGMEM/{f=1;next} f&&/^\)EOF"/{f=0} f' "$INO" \
 | awk -v d="$T" '/-----BEGIN CERTIFICATE-----/{n++} n{print > (d "/ino-" n ".pem")}'

fp() { openssl x509 -in "$1" -noout -fingerprint -sha256 | cut -d= -f2; }
for f in "$T"/ino-*.pem;  do fp "$f"; done | sort > "$T/ino.fp"
for f in certs/*.pem;     do fp "$f"; done | sort > "$T/files.fp"

echo "== sertifikat di .ino ($(wc -l < "$T/ino.fp")) vs certs/ ($(wc -l < "$T/files.fp"))"
if diff "$T/ino.fp" "$T/files.fp"; then echo "OK: identik"; else echo "BEDA"; exit 1; fi

echo "== subjek dan masa berlaku"
for f in certs/*.pem; do
  echo "-- $f"
  openssl x509 -in "$f" -noout -subject -enddate
  openssl x509 -in "$f" -noout -checkend $((730*86400)) >/dev/null \
    && echo "   sisa > 2 tahun" || echo "   PERINGATAN: kedaluwarsa < 2 tahun"
done
