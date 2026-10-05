# Project: [Attendance Machine]

Firmware untuk [ESP32 C3 Supermini], versi saat ini v2.3.3.

## Hardware
- Board: [ESP32C3 Dev Module]
- Sensor/aktuator: [daftar + pin]

## Build & upload
- Library: [daftar + versi]
- Board package: [esp32 by Espressif 3.3.12]
- Perintah: `arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs .`

## Konvensi
- Provisioning: lihat provisioning.png dan README
- Jangan hardcode kredensial WiFi/token; pakai [NVS/config]
- Versi mengikuti semver, tag git `vX.Y.Z`

## Hal yang jangan diubah tanpa tanya
- Pin mapping, format payload, alur provisioning
