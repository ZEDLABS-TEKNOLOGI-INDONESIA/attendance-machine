# Project: Attendance Machine

Firmware untuk ESP32-C3 Super Mini, versi saat ini v2.3.6.

## Hardware
- Board: ESP32-C3 Super Mini (profil Arduino: `ESP32C3 Dev Module`, FQBN `esp32:esp32:esp32c3`)
- Periferal: MFRC522 (RFID), OLED SSD1306 128x64, modul SD card, buzzer pasif, charger 4056 + baterai
- Pin: SCK 4, MOSI 6, MISO 5, RFID SS 7, RFID RST 3, SD CS 1, SDA 8, SCL 9, Buzzer 10, BOOT 0

## Build & upload
- Library:
  - MFRC522 1.4.12
  - Adafruit SSD1306 2.5.17
  - Adafruit GFX Library 1.12.6
  - Adafruit BusIO 1.17.4
  - ArduinoJson 7.4.3 (API v7: `JsonDocument`, `to<JsonArray>()`)
  - SdFat 2.3.0
- Board package: esp32 by Espressif 3.3.12
- Partition scheme: Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS), yaitu `PartitionScheme=min_spiffs`
- Compile: `arduino-cli compile --clean --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs --export-binaries .`
- Upload (USB): `arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs .`
- Cek marker versi: `strings build/esp32.esp32.esp32c3/attendance-machine.ino.bin | grep FWVER:` (harus ada `FWVER:<versi>;`)
- Cek MD5: `md5sum build/esp32.esp32.esp32c3/attendance-machine.ino.bin`
- Warning dari library SdFat/FS.h (`FILE_READ`, `FILE_WRITE`, `__has_include(FS.h)`) aman diabaikan.

## Rilis OTA
1. Naikkan `FIRMWARE_VERSION` dan komentar header sebelum build.
2. Compile dengan `--export-binaries`, cek marker, hitung MD5 dari file `.ino.bin` yang sama.
3. Unggah `.ino.bin` ke server dengan versi persis `x.y.z` dan MD5 tersebut.
4. Verifikasi serial setelah restart: `[FW] FWVER:<versi>;`.
- Server wajib mengirim MD5 (32 hex), URL `https://` dengan host sama seperti API, dan `version` yang sama dengan marker di binary.

## Konvensi
- Provisioning: lihat README.
- Jangan hardcode kredensial WiFi/token. Simpan di NVS terenkripsi (namespace `cfg`).
- Versi mengikuti semver, tag git `vX.Y.Z`.
- Jangan menaruh fungsi apa pun di atas deklarasi `struct`/`enum`: `arduino-cli` menyisipkan prototipe otomatis sebelum fungsi pertama, sehingga tipe belum dikenal dan kompilasi gagal (contoh: `verifyRollbackLater()`).
- Urutan kunci mutex: `xSdMutex` dulu, baru `xCacheMutex`. Jangan memanggil fungsi yang mengambil `xSdMutex` sambil memegang mutex itu (tidak rekursif).
- Akses `Preferences` (NVS) selalu lewat `lockNvs()`/`unlockNvs()`.
- Waktu: pakai `getEpochWithFallback()` untuk semua keperluan epoch, bukan `time(nullptr)` langsung.
- Nama device: maksimum 19 karakter, hanya huruf/angka/spasi/`-`/`_`/`.`.
- Semua request HTTPS membuat `WiFiClientSecure` sendiri lewat `applyTls()`. Root CA ada di `TLS_ROOT_CA`.
