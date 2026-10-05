# Issues

Legenda: **[BUG]** bug nyata dari kode. **[SARAN]** peningkatan/hardening. **[VERIFIKASI]** perlu dicek di perangkat. **[KEPUTUSAN]** perlu keputusan pemilik. **[DOKUMEN]** dokumentasi.

Dasar: `attendance-machine.ino` v2.3.6 (branch `hardening-2.3.6`), `README.md`, `requirements.md`. Penomoran 1-44 mengikuti issue.md lama, 50-66 adalah temuan berikutnya.

**Ringkasan:** semua item BUG dan KEPUTUSAN teratasi di kode. Tinggal 7 item VERIFIKASI lapangan (18, 28, 38, 51, 52, 64, 66) dan 1 saran skala besar (37). Build bersih, OTA 2.3.6 terbukti berhasil di perangkat.

---

## Masih Terbuka (hanya verifikasi lapangan)

### 18. [VERIFIKASI] Rollback OTA belum teruji di perangkat
- Kode sudah benar: `verifyRollbackLater()` override, penandaan valid setelah RC522 OK dan sebelum WiFi. Efektif hanya jika bootloader build mendukung rollback.
- Uji: flash image yang sengaja gagal di RC522, pastikan kembali ke versi lama.

### 28. [VERIFIKASI] `extendWdtForSync()` tidak berefek di `setup()`
- Task belum dibuat saat sync di `setup()`, sehingga WDT tetap 90 detik. Setiap operasi blocking mereset WDT, risiko kecil. Uji backlog besar saat boot.

### 38. [VERIFIKASI] Penyebab disconnect WiFi masih hipotesis
- `WiFi.onEvent` mencatat reason code. Brownout TX burst belum diukur. Jika di lapangan sering lepas, baca `[WIFI] disconnect reason=N` dulu, lalu uji satu variabel per waktu (`WIFI_PS_NONE`, TX power).

### 51. [VERIFIKASI] `useHTTP10(true)` untuk unduhan DB RFID di balik Cloudflare
- OTA terbukti berhasil. Unduhan DB RFID belum diuji. Jika bermasalah, hapus baris `useHTTP10` di `downloadRfidDb()` saja.

### 52. [VERIFIKASI] Handshake HTTPS saat jam sistem belum valid
- Urutan di `setup()` sudah benar: NTP (UDP) berjalan sebelum `pingAPI()` (HTTPS). Sisa yang perlu diuji: WiFi hidup tetapi NTP diblok.

### 64. [VERIFIKASI] Jam sistem bertahan saat `ESP.restart()`
- `isTimeTrusted()` menganggap jam tepercaya jika `time(nullptr)` sudah valid. Pastikan jam sistem selamat dari restart (OTA, retry boot). Jika tidak, scan sesaat setelah restart ditolak `WAKTU BELUM SYNC` sampai NTP berhasil.

### 66. [VERIFIKASI] Backoff polling server
- Uji: (1) matikan backend, interval `[OTA] check HTTP ...` melebar 10, 20, 40, 60 menit; (2) nyalakan lagi, penghitung kembali 0; (3) putus-sambung WiFi, polling kembali normal; (4) sync antrean tetap sesuai `syncIntervalMs`.

### Saran yang diterima sebagai batasan
- **31** Enkripsi kredensial lemah terhadap dump flash. Hardening penuh = flash encryption + NVS encryption ESP-IDF.
- **32** Kartu admin bisa dikloning, `/admin_rfid.txt` plaintext. Dampak terbatas (status dan sync manual).
- **33** `uidToString()` memakai 4 byte pertama UID. Aman jika semua kartu 4 byte. Pastikan aturan sama di server.
- **37** Pemindaian antrean O(n²) terhadap jumlah file. Tidak terasa pada puluhan file. Pertimbangkan file indeks bila antrean bisa ribuan. Ditunda sampai ada data lapangan.
- **20 (sisa)** Form `/save` tidak punya autentikasi selain password AP acak dan jendela 5 menit. Dianggap cukup.

---

## Teratasi

| # | Masalah | Penyelesaian |
|---|---|---|
| 1 | Filter umur memakai `time(nullptr)`, antrean bisa terhapus | `getEpochWithFallback()` satu sumber, file tidak dihapus bila jam tidak valid |
| 2 | `chunkedSync()` macet setelah 5 file | Kuota per siklus di-reset tiap panggilan |
| 3 | Pemindaian berhenti di celah indeks | Iterasi direktori (`findNextQueueFileLocked`) |
| 4 | File dihapus saat masih ditulis | `detachFromWriterLocked()` + `syncingQueueFile` |
| 5 | SD terlepas tidak terdeteksi | Baca sektor 0, 3 kegagalan beruntun |
| 6 | Kredensial panjang gagal tersimpan | Blok 96 byte, validasi form, format lama tetap terbaca |
| 7 | TLS tanpa verifikasi | Root CA tertanam, OTA dibatasi host API |
| 8 | OTA menerima image terpotong / hang | MD5 wajib, cek `Update.write`, stall 30 dtk, marker versi |
| 9 | Konflik pin SCL dan BOOT | `PIN_BOOT` = GPIO 0 |
| 10 | SPI tanpa mutex di `loop()` | `pollRfidReader()` di bawah `xSdMutex` |
| 11 | `getLocalTime()` memblokir 5 detik | Jalur epoch tanpa blocking |
| 12 | Mutex SD dipegang saat streaming DB | Mutex hanya saat menulis blok |
| 13 | Race cache RFID | `xCacheMutex`, timeout lookup 2,5 detik |
| 14 | `Preferences` lintas task | `xNvsMutex` |
| 15 | Config `null` tidak kembali ke default | Default firmware diterapkan dan dipersist |
| 16 | Durasi sleep salah untuk jendela non-wrap | Perhitungan berdasarkan jenis jendela |
| 17 | Timezone belum di-set di jalur fallback | `setenv("TZ","WIB-7")` di awal `setup()` |
| 19 | Kredensial WiFi plaintext di driver | `persistent(false)` + `esp_wifi_restore()` saat reset |
| 20 | Provisioning lemah | Password AP acak di OLED, `https://` wajib |
| 21 | Batas nama device tidak konsisten | 19 karakter di form, kode, dan README |
| 22 | Koma/kutip di nama device merusak CSV/JSON | Karakter dibatasi di form dan disanitasi saat load |
| 23 | `WiFiClientSecure` bersama | Klien per request (`applyTls`) |
| 24 | Buffer URL | `buildApiUrl()` dengan cek panjang |
| 25 | Cache 5000 tanpa peringatan | Jumlah terbuang tampil di OLED dan heartbeat |
| 26 | Validasi panjang dekripsi | `decryptBytes()` menolak `len` > isi blob |
| 27 | Log dari dalam mutex SD | Kode mati dihapus |
| 29 | ArduinoJson v6 vs v7 | Kode v7, compile bersih |
| 30 | Versi tidak sinkron / loop OTA | v2.3.6, marker `FWVER` dicocokkan |
| 34 | DB belum ada menolak semua kartu | `RFID_NO_DB` tetap menyimpan, server memvalidasi |
| 35 | Default dim OLED tidak sinkron (kode 08-10, dokumen 08-12) | Kode jadi acuan: 08:00-10:00. OLED menyala saat scan. Nonaktif default: set start == end |
| 36 | Sleep tidak cek OTA, RC522 tetap hidup | `otaInProgress` dicek, `PCD_SoftPowerDown()` |
| 39 | Validasi jam sleep/dim | Form menolak nilai salah, `start == end` = nonaktif (disengaja), log serial |
| 40 | Polling server tanpa backoff | Backoff eksponensial (maks 16x, 1 jam) untuk OTA/DB/heartbeat/config; reset saat sukses atau WiFi tersambung ulang |
| 41 | `failed_log` berhenti di 500 baris | Rotasi ke `.old` |
| 42 | DB kosong dianggap gagal | Daftar kosong yang sah (ada `END`) diterima |
| 43-44 | README tidak sinkron | README dan requirements ditulis ulang |
| 45-49 | Teratasi sebelumnya | Sleep non-wrap, DB tanpa `END`, log gagal, busy-spin `loop()`, shadowing |
| 50 | Jam setelah cold boot mengabaikan lama mati | RTC opsional (DS1307/DS3231, sudah ada) + `isTimeTrusted()`: scan dan sleep ditahan sampai NTP/RTC, retry NTP tiap 60 dtk, OLED `--:--`. Saklar `REJECT_SCAN_UNTRUSTED_TIME` |
| 53 | Factory reset tidak menghapus `queue_N.csv` | Tahan BOOT 5 dtk = reset biasa, 10 dtk = reset + hapus antrean. Reset berjalan setelah tombol dilepas |
| 54 | Rollback OTA tidak pernah aktif | `verifyRollbackLater()` return true, penandaan manual setelah RC522 OK |
| 55 | Tap admin tidak langsung sync | `forceSyncRequested` dikonsumsi `taskSync` |
| 56 | Scan ditolak "CACHE SIBUK" saat reload DB | Timeout lookup 300 ms menjadi 2500 ms |
| 57 | Penggantian DB RFID tidak atomik | Swap lewat `.bak`, pemulihan saat boot |
| 58 | Respons config 200 bukan objek JSON mereset ke default | Guard `is<JsonObject>()` |
| 59 | Counter retry boot di RTC bisa ter-reset | Counter di NVS (`boot_retry`) |
| 60 | Pemindaian antrean menahan mutex SD | Mutex per file, `queueMutations`, metadata hanya saat pindah file |
| 61 | `FW_MARKER` dibuang linker | `used, retain` + referensi di `setup()` |
| 62 | Fungsi di atas `struct` merusak prototipe otomatis `arduino-cli` | Aturan penempatan dicatat di requirements.md. Variabel backoff memakai `uint8_t` biasa |
| 63 | Unduhan streaming rentan chunked encoding | `useHTTP10(true)` (DB RFID menunggu verifikasi, lihat 51) |
| 65 | [DOKUMEN] Factory reset selalu menghapus buffer offline NVS (`rec_N`) | Perilaku lama, kini didokumentasikan di README. Kirim dulu (tap kartu admin) sebelum reset |

---

## Rekomendasi Urutan Berikutnya
1. Pilot 1-2 unit selama seminggu (satu dengan DS3231, satu tanpa), lalu tutup item **18, 51, 52, 28, 38, 64, 66** dengan data lapangan.
2. Pertimbangkan **37** hanya jika ada unit yang menumpuk ratusan file antrean.
3. Jika lokasi punya WiFi tidak stabil, pasang DS3231 agar scan tidak ditolak setelah mati daya.
