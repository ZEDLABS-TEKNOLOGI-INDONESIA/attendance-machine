set -euo pipefail
INO="${1:-src/tls.h}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

awk -v d="$TMP" '
  /-----BEGIN CERTIFICATE-----/ {n++; f=sprintf("%s/ino_%02d.pem", d, n); on=1}
  on {print > f}
  /-----END CERTIFICATE-----/ {on=0; close(f)}
' "$INO"

fp() { openssl x509 -in "$1" -noout -fingerprint -sha256 | cut -d= -f2; }

echo "== Fingerprint di .ino =="
for f in "$TMP"/ino_*.pem; do echo "$(basename "$f")  $(fp "$f")"; done

echo "== Fingerprint di certs/ =="
for f in certs/*.pem; do echo "$f  $(fp "$f")"; done

echo "== Pencocokan =="
rc=0
for f in certs/*.pem; do
  want="$(fp "$f")"; hit=0
  for g in "$TMP"/ino_*.pem; do [ "$(fp "$g")" = "$want" ] && hit=1; done
  if [ $hit -eq 1 ]; then echo "OK    $f ada di .ino"; else echo "BEDA  $f TIDAK ada di .ino"; rc=1; fi
done

echo "== Kedaluwarsa =="
for f in certs/*.pem; do
  openssl x509 -in "$f" -noout -subject -enddate
  if openssl x509 -in "$f" -noout -checkend $((90*86400)); then echo "  >90 hari lagi"; else echo "  PERINGATAN: <90 hari"; rc=1; fi
done
exit $rc
