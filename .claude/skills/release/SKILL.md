---
name: release
description: Siapkan rilis firmware baru (naik versi, compile, tag git, siapkan binary OTA). Gunakan saat user bilang "rilis", "bump versi", atau "siapkan OTA".
disable-model-invocation: true
---

# Release

1. Tanyakan versi baru (semver). Ubah serentak: FIRMWARE_VERSION, header komentar, tabel di README.md, tanggal Updated.
2. Pastikan bagian OTA kompatibel: compareFirmwareVersion hanya membaca x.y.z.
3. Compile bersih di Arduino IDE 2.3.6 / core 3.3.12, partition Minimal SPIFFS. Pastikan ukuran sketch muat di slot OTA (~1.9MB).
4. Export binary (.bin) dan hitung MD5 (backend /firmware/check butuh md5).
5. Uji di 1 device fisik: boot, WiFi, ping API, tap kartu, sync, OTA dari versi lama.
6. git commit, `git tag vX.Y.Z`, GitHub release, upload binary ke backend.
7. Catat perubahan protokol ke backend di notes rilis.
