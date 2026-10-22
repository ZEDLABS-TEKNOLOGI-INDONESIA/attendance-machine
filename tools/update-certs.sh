#!/usr/bin/env bash
# Kelola root CA tertanam tanpa copy-paste.
#
#   tools/update-certs.sh add <nama> <url|berkas> [--fp SHA256] [--dry-run]
#   tools/update-certs.sh remove <nama> [--dry-run]
#   tools/update-certs.sh sync [--dry-run]
#   tools/update-certs.sh refresh [--offline] [--dry-run]
#
# add    : unduh (atau salin dari berkas lokal), validasi, simpan ke certs/<nama>.pem,
#          lalu tulis ulang blok TLS_ROOT_CA di src/tls.h dari SEMUA certs/*.pem.
# remove : hapus certs/<nama>.pem lalu tulis ulang blok TLS_ROOT_CA.
# sync   : tulis ulang blok TLS_ROOT_CA dari certs/*.pem (mis. setelah edit manual).
#
# refresh: bandingkan tiap entri certs/sources.txt dengan sumber resminya (harus identik,
#          selisih = berhenti), lalu sync. Dipakai otomatis oleh tools/build.sh.
#
# Skrip TIDAK menaikkan FIRMWARE_VERSION dan TIDAK mengompilasi.
set -euo pipefail
cd "$(dirname "$0")/.."

TLS_H="${TLS_H:-src/tls.h}"
CERT_DIR="${CERT_DIR:-certs}"
WARN_DAYS=365
DRY=0

TMPD="$(mktemp -d)"
trap 'rm -rf "$TMPD"' EXIT

die()  { echo "ERROR: $*" >&2; exit 1; }
warn() { echo "PERINGATAN: $*" >&2; }

command -v openssl >/dev/null || die "openssl tidak ditemukan"
[ -f "$TLS_H" ] || die "$TLS_H tidak ditemukan"

fp() { openssl x509 -in "$1" -noout -fingerprint -sha256 | cut -d= -f2; }
norm() { echo "$1" | tr -d ': \n' | tr 'a-f' 'A-F'; }

validate() {
  local f="$1" s i
  openssl x509 -in "$f" -noout 2>/dev/null || die "$f bukan sertifikat PEM yang valid"
  openssl x509 -in "$f" -noout -text | grep -q 'CA:TRUE' || die "$f bukan sertifikat CA"
  s="$(openssl x509 -in "$f" -noout -subject | sed 's/^subject= *//')"
  i="$(openssl x509 -in "$f" -noout -issuer  | sed 's/^issuer= *//')"
  [ "$s" = "$i" ] || die "$f bukan root (subject != issuer): ini sertifikat perantara/leaf"
  openssl x509 -in "$f" -noout -checkend 0 >/dev/null || die "$f SUDAH kedaluwarsa"
  openssl x509 -in "$f" -noout -checkend $((WARN_DAYS * 86400)) >/dev/null \
    || warn "$f kedaluwarsa dalam < $WARN_DAYS hari"
}

sync_tls() {
  shopt -s nullglob
  local files=("$CERT_DIR"/*.pem)
  [ ${#files[@]} -gt 0 ] || die "tidak ada $CERT_DIR/*.pem"
  local blk new f n
  blk="$TMPD/blk"; new="$TMPD/new"; : > "$blk"
  for f in "${files[@]}"; do
    validate "$f"
    openssl x509 -in "$f" -outform PEM >> "$blk"
  done
  n="$(grep -c 'BEGIN CERTIFICATE' "$blk")"
  [ "$(grep -c 'R"EOF($' "$TLS_H")" -eq 1 ] || die "$TLS_H: penanda R\"EOF( harus tepat satu"
  [ "$(grep -c '^)EOF";' "$TLS_H")" -eq 1 ] || die "$TLS_H: penanda )EOF\"; harus tepat satu"
  awk -v blk="$blk" '
    skip == 0 { print }
    skip == 0 && /R"EOF\($/ { while ((getline l < blk) > 0) print l; skip = 1; next }
    skip == 1 && /^\)EOF";/ { print; skip = 0 }
  ' "$TLS_H" > "$new"
  [ "$(grep -c 'BEGIN CERTIFICATE' "$new")" -eq "$n" ] || die "hasil tulis ulang tidak konsisten, dibatalkan"
  if cmp -s "$TLS_H" "$new"; then
    echo "Tidak ada perubahan pada $TLS_H ($n sertifikat)."
    return 0
  fi
  diff -u "$TLS_H" "$new" | grep -vE '^[+-][A-Za-z0-9+/=]{40,}$' | head -40 || true
  if [ "$DRY" -eq 1 ]; then
    echo "Dry-run: $TLS_H TIDAK diubah ($n sertifikat akan ditanam)."
    return 0
  fi
  cat "$new" > "$TLS_H"
  echo "OK: $TLS_H diperbarui ($n sertifikat)."
  [ "${QUIET:-0}" = 1 ] && return 0
  echo "--- verifikasi ---"
  bash tools/verify-certs.sh "$TLS_H" || warn "verify-certs.sh melaporkan masalah (lihat di atas)"
  echo
  echo "Langkah berikutnya: naikkan FIRMWARE_VERSION di src/config.h, compile, uji pilot, rilis OTA."
}

cmd_add() {
  local name="${1:-}" src="${2:-}" want=""
  shift 2 || die "pemakaian: add <nama> <url|berkas> [--fp SHA256]"
  while [ $# -gt 0 ]; do
    case "$1" in
      --fp) want="$(norm "${2:-}")"; shift 2 ;;
      --dry-run) DRY=1; shift ;;
      *) die "opsi tidak dikenal: $1" ;;
    esac
  done
  [[ "$name" =~ ^[a-z0-9_-]+$ ]] || die "nama hanya boleh a-z 0-9 _ -"
  [ ! -e "$CERT_DIR/$name.pem" ] || die "$CERT_DIR/$name.pem sudah ada (hapus dulu dengan 'remove')"
  local raw="$TMPD/raw" pem="$TMPD/pem"
  if [ -f "$src" ]; then
    cp "$src" "$raw"
  else
    [[ "$src" == https://* ]] || die "sumber harus https:// atau berkas lokal"
    command -v curl >/dev/null || die "curl tidak ditemukan"
    curl -fsSL --proto '=https' --tlsv1.2 --max-time 30 -o "$raw" "$src" || die "unduh gagal: $src"
  fi
  openssl x509 -in "$raw" -out "$pem" 2>/dev/null \
    || openssl x509 -inform DER -in "$raw" -out "$pem" 2>/dev/null \
    || die "berkas bukan sertifikat X.509 (PEM/DER)"
  validate "$pem"
  local got; got="$(fp "$pem")"
  echo "Subject    : $(openssl x509 -in "$pem" -noout -subject | sed 's/^subject= *//')"
  echo "Berlaku s/d: $(openssl x509 -in "$pem" -noout -enddate | cut -d= -f2)"
  echo "SHA-256    : $got"
  local f
  for f in "$CERT_DIR"/*.pem; do
    [ -e "$f" ] || continue
    [ "$(fp "$f")" != "$got" ] || die "sertifikat ini sudah ada sebagai $f"
  done
  if [ -n "$want" ]; then
    [ "$(norm "$got")" = "$want" ] || die "SHA-256 TIDAK cocok dengan --fp. Dibatalkan."
    echo "SHA-256 cocok dengan --fp."
  else
    [ -t 0 ] || die "tanpa terminal: wajib pakai --fp <SHA256 dari halaman resmi CA>"
    echo "Bandingkan SHA-256 di atas dengan halaman resmi CA."
    read -r -p "Cocok? Ketik YA untuk melanjutkan: " ans
    [ "$ans" = "YA" ] || die "dibatalkan"
  fi
  if [ "$DRY" -eq 1 ]; then
    echo "Dry-run: tidak menulis $CERT_DIR/$name.pem"
    return 0
  fi
  mkdir -p "$CERT_DIR"
  cp "$pem" "$CERT_DIR/$name.pem"
  echo "Disimpan: $CERT_DIR/$name.pem"
  if [[ "$src" == https://* ]]; then
    echo "$name $src" >> "$CERT_DIR/sources.txt"
    echo "Dicatat di $CERT_DIR/sources.txt (dipakai oleh 'refresh')."
  fi
  sync_tls
}

cmd_remove() {
  local name="${1:-}"; shift || true
  [ "${1:-}" != "--dry-run" ] || DRY=1
  [ -f "$CERT_DIR/$name.pem" ] || die "$CERT_DIR/$name.pem tidak ada"
  local left; left="$(ls "$CERT_DIR"/*.pem | grep -vc "/$name.pem$" || true)"
  [ "$left" -ge 1 ] || die "tidak boleh menghapus sertifikat terakhir"
  echo "Menghapus $name. Bila server/Cloudflare MASIH memakai root ini, seluruh HTTPS (termasuk OTA) mesin gagal."
  if [ "$DRY" -eq 0 ]; then
    [ -t 0 ] || die "butuh terminal untuk konfirmasi"
    read -r -p "Ketik YA untuk melanjutkan: " ans
    [ "$ans" = "YA" ] || die "dibatalkan"
    rm "$CERT_DIR/$name.pem"
    [ ! -f "$CERT_DIR/sources.txt" ] || sed -i "/^$name /d" "$CERT_DIR/sources.txt"
  else
    echo "Dry-run: berkas tidak dihapus, menampilkan hasil tanpa $name"
    return 0
  fi
  sync_tls
}

cmd_refresh() {
  local offline=0 a list="$CERT_DIR/sources.txt" name url raw pem
  for a in "$@"; do
    case "$a" in --offline) offline=1 ;; --dry-run) DRY=1 ;; *) die "opsi tidak dikenal: $a" ;; esac
  done
  if [ "$offline" -eq 0 ] && [ -f "$list" ] && command -v curl >/dev/null; then
    while read -r name url _; do
      [[ -z "${name:-}" || "$name" == \#* ]] && continue
      [ -f "$CERT_DIR/$name.pem" ] || { warn "$name ada di sources.txt tetapi $CERT_DIR/$name.pem tidak ada"; continue; }
      raw="$TMPD/r_$name"; pem="$TMPD/p_$name"
      if ! curl -fsSL --proto '=https' --tlsv1.2 --max-time 20 -o "$raw" "$url" 2>/dev/null; then
        warn "gagal mengunduh $name ($url); memakai salinan lokal"; continue
      fi
      if ! { openssl x509 -in "$raw" -out "$pem" 2>/dev/null || openssl x509 -inform DER -in "$raw" -out "$pem" 2>/dev/null; }; then
        warn "$name: respons bukan sertifikat; memakai salinan lokal"; continue
      fi
      if [ "$(fp "$pem")" = "$(fp "$CERT_DIR/$name.pem")" ]; then
        echo "OK    $name sama dengan sumber resmi"
      else
        die "$name: sertifikat di sumber resmi BERBEDA dari $CERT_DIR/$name.pem. Tinjau manual (bandingkan sidik jari di halaman resmi CA), lalu pakai 'remove' dan 'add'."
      fi
    done < "$list"
  elif [ "$offline" -eq 0 ]; then
    warn "tidak ada $list atau curl; hanya sync lokal"
  fi
  sync_tls
}

case "${1:-}" in
  refresh) shift; cmd_refresh "$@" ;;
  add)    shift; cmd_add "$@" ;;
  remove) shift; cmd_remove "$@" ;;
  sync)   shift; [ "${1:-}" != "--dry-run" ] || DRY=1; sync_tls ;;
  *) sed -n '2,17p' "$0"; exit 2 ;;
esac
