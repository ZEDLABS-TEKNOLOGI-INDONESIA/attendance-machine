#!/usr/bin/env python3
"""
Memecah attendance-machine.ino (monolit) menjadi modul header di src/.

Prinsip: PEMOTONGAN MEKANIS, tanpa mengubah satu baris kode pun.
  - Urutan kode asli dipertahankan; tiap berkas = rentang baris berurutan.
  - Semua header di-include sekali dari .ino (satu translation unit),
    jadi binary identik secara logika dan tidak ada masalah linkage.
  - Skrip memverifikasi: gabungan semua modul (selain baris kosong) == aslinya.

Pemakaian (dari root repo):
  python3 tools/split_modules.py                # dry-run: tampilkan rencana + verifikasi
  python3 tools/split_modules.py --apply        # tulis src/*.h dan .ino baru
  python3 tools/split_modules.py path/ke.ino --apply
"""
import os
import sys

args = [a for a in sys.argv[1:] if not a.startswith("--")]
APPLY = "--apply" in sys.argv
SRC_INO = args[0] if args else "attendance-machine.ino"
OUT_DIR = "src"

# (nama berkas, baris pembuka [awalan baris, harus unik], deskripsi)
SECTIONS = [
    ("config.h",         None,                                                   "Include, makro konfigurasi, makro log"),
    ("types.h",          "__attribute__((used, retain)) static const char FW_MARKER", "Marker versi, RTC_DATA_ATTR, enum, struct"),
    ("globals.h",        "Adafruit_SSD1306 display(",                            "Objek global, variabel global, lock NVS/cache"),
    ("credentials.h",    "static void deriveAesKey",                             "Enkripsi kredensial, load/simpan config, runtime config"),
    ("tls.h",            "#define TLS_VERIFY_CERT 1",                           "Root CA dan applyTls()"),
    ("hw_helpers.h",     "inline bool acquireSD",                                "Mutex SD, select/deselect, helper WiFi/sinyal"),
    ("watchdog.h",       "static bool wdtApply",                                 "Pengaturan WDT"),
    ("ui_feedback.h",    "void turnOffOLED()",                                   "OLED on/off, showOLED, progress, buzzer"),
    ("nvs_store.h",      "void nvsSaveLastTime(time_t t)",                       "NVS: waktu, scan count, boot retry, helper int"),
    ("diag.h",           "static int diagBootCount",                             "Diagnosis boot dan pelacakan OTA"),
    ("nvs_records.h",    "int nvsGetCount()",                                    "NVS: buffer record, versi DB, last scan"),
    ("rfid_db.h",        "static void clearRfidCacheRaw",                        "Cache RFID, daftar admin"),
    ("rtc_time.h",       "static uint8_t bcd2bin",                               "RTC eksternal, epoch, SNTP, jadwal OLED"),
    ("sd_log.h",         "static bool prepareAppendLocked",                      "Append aman dan failed_log"),
    ("queue_files.h",    "void getQueueFileName(int",                            "Nama/iterasi file antrean, metadata"),
    ("sd_health.h",      "bool reinitSDCard()",                                  "Reinit SD, health check, flush"),
    ("queue_scan.h",     "bool fileHasValidRecords(",                            "Pemindaian antrean, duplikat, klasifikasi baris"),
    ("quarantine.h",     "static bool openQuarantineLocked",                     "Karantina, retain record, pemulihan tmp"),
    ("queue_write.h",    "bool initSDCard()",                                    "Init SD dan saveToQueue"),
    ("nvs_sync.h",       "static bool nvsCompactAfterSync",                      "Sync buffer NVS, parseSyncReply"),
    ("backoff.h",        "static uint8_t failOta",                               "Penghitung backoff polling"),
    ("rfid_db_sync.h",   "unsigned long checkRfidDbVersion()",                   "Cek versi dan unduh DB RFID"),
    ("telemetry.h",      "void sendTelemetry()",                                 "Heartbeat"),
    ("remote_config.h",  "static const RuntimeConfig kRuntimeDefaults",          "Remote config"),
    ("ota.h",            "static bool extractUrlHost",                           "OTA"),
    ("queue_sync.h",     "bool readQueueFileLocked(",                            "Sync antrean SD ke server"),
    ("wifi_net.h",       "static void onWifiEvent",                              "WiFi, ping, state machine reconnect"),
    ("scan.h",           "void uidToString(uint8_t",                             "UID, kirim presensi, kirimPresensi"),
    ("ui_standby.h",     "bool displayStateChanged()",                           "Layar standby dan animasi startup"),
    ("factory_reset.h",  "static int wipeQueueFiles()",                          "Factory reset"),
    ("provisioning.h",   "static String provHtmlPage()",                         "Provisioning AP dan halaman web"),
    ("tasks.h",          "void taskRfid(void",                                   "Task FreeRTOS: rfid, sync, display"),
]
TAIL_ANCHOR = "static void logNvsStats()"  # sisanya tetap di .ino: setup/loop

FORWARD_H = """#pragma once
// Deklarasi maju. Dulu dibuat otomatis oleh arduino-cli untuk fungsi di .ino;
// sekarang kode ada di header, jadi fungsi yang dipakai sebelum didefinisikan
// harus dideklarasikan manual di sini.
bool getTimeWithFallback(struct tm *ti);
"""
# header yang disisipkan setelah globals.h (urutan include)
AFTER_GLOBALS = "forward.h"

text = open(SRC_INO, encoding="utf-8").read()
lines = text.split("\n")


def find_unique(anchor):
    hits = [i for i, l in enumerate(lines) if l.startswith(anchor)]
    if len(hits) != 1:
        sys.exit(f"ERROR: anchor '{anchor}' cocok {len(hits)} kali (harus 1)")
    return hits[0]


starts = []
for name, anchor, _ in SECTIONS:
    starts.append(0 if anchor is None else find_unique(anchor))
tail_start = find_unique(TAIL_ANCHOR)
if starts != sorted(starts) or len(set(starts)) != len(starts) or starts[-1] >= tail_start:
    sys.exit("ERROR: urutan anchor tidak menaik; file berbeda dari yang diharapkan")

bounds = starts + [tail_start]
bodies = {}
for i, (name, _, _) in enumerate(SECTIONS):
    chunk = lines[bounds[i]:bounds[i + 1]]
    while chunk and chunk[-1].strip() == "":
        chunk.pop()
    bodies[name] = chunk
tail = lines[tail_start:]

# Verifikasi: gabungan (tanpa baris kosong) harus identik dengan aslinya
joined = [l for n, _, _ in SECTIONS for l in bodies[n]] + tail
if [l for l in joined if l.strip()] != [l for l in lines if l.strip()]:
    sys.exit("ERROR: verifikasi gagal, gabungan modul != aslinya")

print(f"Sumber: {SRC_INO} ({len(lines)} baris). Verifikasi gabungan == asli: OK\n")
for i, (name, _, desc) in enumerate(SECTIONS):
    print(f"  src/{name:<18} {len(bodies[name]):>5} baris  {desc}")
print(f"  {os.path.basename(SRC_INO):<22} {len(tail):>5} baris  setup(), loop(), poll RFID, hitung tidur")

if not APPLY:
    print("\nDry-run. Tambahkan --apply untuk menulis berkas.")
    sys.exit(0)

os.makedirs(OUT_DIR, exist_ok=True)
for name, _, desc in SECTIONS:
    with open(os.path.join(OUT_DIR, name), "w", encoding="utf-8", newline="\n") as f:
        if name != "config.h":
            f.write("#pragma once\n")
        else:
            f.write("#pragma once\n")
        f.write("\n".join(bodies[name]) + "\n")
with open(os.path.join(OUT_DIR, AFTER_GLOBALS), "w", encoding="utf-8", newline="\n") as f:
    f.write(FORWARD_H)

order = []
for name, _, _ in SECTIONS:
    order.append(name)
    if name == "globals.h":
        order.append(AFTER_GLOBALS)

banner = (
    "/*\n"
    " * ATTENDANCE MACHINE (ESP32-C3 Super Mini)\n"
    " * Kode dipecah menjadi modul di src/ (lihat README, bagian Struktur Kode).\n"
    " * Versi firmware: FIRMWARE_VERSION di src/config.h.\n"
    " * ATURAN: urutan #include di bawah adalah urutan dependensi. Jangan diubah.\n"
    " */\n"
)
with open(SRC_INO, "w", encoding="utf-8", newline="\n") as f:
    f.write(banner)
    for n in order:
        f.write(f'#include "src/{n}"\n')
    f.write("\n")
    f.write("\n".join(tail))
    if not tail[-1] == "":
        f.write("\n")

print(f"\nSelesai. Ditulis: {OUT_DIR}/ ({len(order)} berkas) dan {SRC_INO} baru.")
print("Berikutnya: git mv pure_logic.h src/pure_logic.h, perbarui include test, lalu compile.")
