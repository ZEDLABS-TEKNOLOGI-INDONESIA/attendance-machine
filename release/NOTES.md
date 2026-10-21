## Attendance Machine v2.3.6

Rilis hardening ESP32-C3.

### Baru
- Waktu tepercaya: scan dan deep sleep ditahan sampai RTC/NTP berhasil (`WAKTU BELUM SYNC`); retry NTP tiap 60 detik.
- RTC eksternal opsional (DS3231/DS1307).
- Factory reset dua tingkat: tahan BOOT 5 detik = reset, 10 detik = reset + hapus antrean.
- Backoff eksponensial untuk polling OTA, versi DB RFID, heartbeat, dan remote config.

### Perbaikan
- Konkurensi (mutex SD/NVS/cache), TLS terverifikasi, OTA aman (MD5 + marker versi), penggantian DB RFID atomik.

### Aset
- `attendance-machine-v2.3.6.bin`: untuk OTA (unggah ke server).
- `attendance-machine-v2.3.6.merged.bin`: flash penuh via USB di alamat 0x0.
- `SHA256SUMS`, `.md5`: checksum.

### Belum diverifikasi di perangkat
Rollback OTA, `useHTTP10` untuk unduhan DB RFID di balik Cloudflare, handshake HTTPS saat jam belum valid, WDT saat sync backlog besar, jam bertahan saat restart, backoff polling.
