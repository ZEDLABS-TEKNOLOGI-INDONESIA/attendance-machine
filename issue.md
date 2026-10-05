# Issues (status setelah Tahap 1-5)

Legenda: **[BUG]** bug nyata dari kode. **[SARAN]** peningkatan/hardening. **[VERIFIKASI]** perlu dicek di perangkat. **[KEPUTUSAN]** perlu keputusan pemilik. **[DOKUMEN]** dokumentasi.

Dasar: `attendance-machine.ino` v2.3.6 (branch `hardening-2.3.4`), `README.md`, `requirements.md` terbaru. Penomoran 1-44 mengikuti issue.md lama, 50-59 adalah temuan baru.

**Ringkasan:** 44 item lama: 36 teratasi, 8 masih terbuka atau menunggu verifikasi. 10 temuan baru: 9 teratasi, 1 terbuka. Build bersih, OTA 2.3.6 terbukti berhasil di perangkat.

---

## Masih Terbuka

### 50. [BUG] Jam setelah listrik mati total mengabaikan lama mati (BARU, risiko tinggi)
- Lokasi: `setup()`. Tanpa NTP, jam dipulihkan dari `last_time` di NVS (waktu sync NTP terakhir) lalu berjalan dengan `bootTime = millis()`. Lama listrik mati tidak terhitung.
- Dampak: setelah mati daya dan WiFi belum tersedia, `timestamp` presensi tertinggal sebesar lama mati (hingga batas estimasi 12 jam). Presensi tersimpan dengan jam salah, dan jendela sleep/dim ikut bergeser.
- Opsi: (a) modul RTC DS3231 (paling andal), (b) tolak scan ("WAKTU BELUM SYNC") setelah cold boot sampai NTP berhasil, (c) tandai record dengan flag "waktu estimasi" agar server bisa meninjau.
- Status: belum dikerjakan, menunggu keputusan.

### 18. [VERIFIKASI] Rollback OTA belum teruji di perangkat
- Kode sudah benar: `verifyRollbackLater()` override, penandaan valid setelah RC522 OK dan sebelum WiFi. Efektif hanya jika bootloader build mendukung rollback.
- Uji: flash image yang sengaja gagal di RC522, pastikan kembali ke versi lama.

### 51. [VERIFIKASI] `useHTTP10(true)` untuk unduhan DB RFID di balik Cloudflare
- OTA terbukti berhasil. Unduhan DB RFID belum diuji dengan perubahan ini. Jika bermasalah, hapus baris `useHTTP10` di `downloadRfidDb()` saja.

### 52. [VERIFIKASI] Handshake HTTPS saat jam sistem belum valid
- Verifikasi sertifikat bergantung pada waktu. Setelah cold boot tanpa NTP, request HTTPS dapat gagal. Perlu diuji (boot dengan WiFi hidup tetapi NTP diblok).

### 28. [VERIFIKASI] `extendWdtForSync()` tidak berefek di `setup()`
- Task belum dibuat saat sync di `setup()`, sehingga WDT tetap 90 detik. Setiap operasi blocking mereset WDT, risiko kecil. Uji backlog besar saat boot.

### 38. [VERIFIKASI] Penyebab disconnect WiFi masih hipotesis
- `WiFi.onEvent` sudah mencatat reason code. Brownout TX burst belum diukur. Jika di lapangan sering lepas, baca `[WIFI] disconnect reason=N` dulu, lalu uji satu variabel per waktu (`WIFI_PS_NONE`, TX power).

### 35. [KEPUTUSAN] Default dim OLED 08:00-12:00
- Logika sudah benar (`isInWindow`), tetapi default mematikan OLED di jam sibuk. Ubah `OLED_DIM_START_HOUR_DEFAULT` dan `OLED_DIM_END_HOUR_DEFAULT`. Nilai ini juga menjadi default saat server mengirim `null`.

### 53. [KEPUTUSAN] Factory reset tidak menghapus `queue_N.csv`
- Disengaja agar data presensi belum terkirim tidak hilang. Konsekuensi: kartu SD yang dipindah ke device lain membawa antrean lama (`device_id` di record tetap nama lama). Putuskan apakah perlu opsi hapus antrean.

### Saran yang diterima sebagai batasan
- **31** Enkripsi kredensial lemah terhadap dump flash. README sudah jujur. Hardening penuh = flash encryption + NVS encryption ESP-IDF.
- **32** Kartu admin bisa dikloning, `/admin_rfid.txt` plaintext. Dampak terbatas (status dan sync manual).
- **33** `uidToString()` memakai 4 byte pertama UID. Aman jika semua kartu 4 byte. Pastikan aturan sama di server.
- **37** Pemindaian antrean kini melepas mutex per file, tetapi biayanya O(n²) terhadap jumlah file. Tidak terasa pada puluhan file. Pertimbangkan file indeks bila antrean bisa ribuan.
- **40** Interval polling sudah dilonggarkan (OTA 10 menit, DB 5 menit, heartbeat 2 menit). Backoff saat server gagal belum ada.
- **20 (sisa)** Form `/save` tidak punya autentikasi selain password AP acak dan jendela 5 menit. Dianggap cukup.

---

## Teratasi (Tahap sebelum 1 dan Tahap 1-5)

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
| 36 | Sleep tidak cek OTA, RC522 tetap hidup | `otaInProgress` dicek, `PCD_SoftPowerDown()` |
| 39 | Validasi jam sleep/dim | Form menolak nilai salah, `start == end` = nonaktif (disengaja), log serial |
| 41 | `failed_log` berhenti di 500 baris | Rotasi ke `.old` |
| 42 | DB kosong dianggap gagal | Daftar kosong yang sah (ada `END`) diterima |
| 43-44 | README tidak sinkron | README dan requirements ditulis ulang untuk v2.3.6 |
| 45-49 | Teratasi sebelumnya | Sleep non-wrap, DB tanpa `END`, log gagal, busy-spin `loop()`, shadowing |

### Temuan baru yang sudah diselesaikan

| # | Masalah | Penyelesaian |
|---|---|---|
| 54 | Rollback OTA tidak pernah aktif: core Arduino menandai valid sendiri sebelum `setup()` | `verifyRollbackLater()` return true, penandaan manual setelah RC522 OK |
| 55 | Tap admin tidak langsung sync (`taskSync` menunggu interval) | `forceSyncRequested` dikonsumsi `taskSync` |
| 56 | Scan ditolak "CACHE SIBUK" saat reload DB | Timeout lookup 300 ms menjadi 2500 ms |
| 57 | Penggantian DB RFID tidak atomik | Swap lewat `.bak`, pemulihan saat boot |
| 58 | Respons config 200 yang bukan objek JSON mereset ke default | Guard `is<JsonObject>()` |
| 59 | `bootTimeSyncRetryCount` di RTC bisa ter-reset, restart tanpa akhir | Counter di NVS (`boot_retry`) |
| 60 | Pemindaian antrean menahan mutex SD, metadata ditulis tiap scan | Mutex per file, `queueMutations`, metadata hanya saat pindah file |
| 61 | `FW_MARKER` dibuang linker | `used, retain` + pointer `volatile` + referensi di `setup()` |
| 62 | Fungsi di atas `struct` merusak prototipe otomatis `arduino-cli` | Aturan penempatan dicatat di requirements.md |
| 63 | Unduhan streaming rentan chunked encoding | `useHTTP10(true)` (DB RFID menunggu verifikasi, lihat 51) |

---

## Rekomendasi Urutan Berikutnya
1. Putuskan item **50** (DS3231 atau tolak scan saat waktu belum sync), ini risiko data paling nyata.
2. Putuskan item **35** (default dim) sebelum dipasang.
3. Pilot 1-2 unit selama seminggu, lalu tutup item **18, 51, 52, 28, 38** dengan data lapangan.
