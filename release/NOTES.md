# v2.3.7
## Perubahan
- WDT: satu panggilan esp_task_wdt_reconfigure, hasil dicek (item 28)
- Diagnosis: reset reason, boot/abnormal count, heap min, stack HWM (serial + heartbeat)
- RC522 gagal: 3 restart lalu mode terbatas; OTA gagal/rollback tidak diulang (ota_bad)
- Handshake deep sleep, perbaikan underflow timer taskSync, batas atas interval remote config (5 dtk-24 jam)
- Daftar admin dimuat atomik; log berlevel (LOG_LEVEL)
## Breaking change
- Tidak ada perubahan format CSV/NVS lama. Kunci NVS baru: boot_cnt abn_cnt last_abn rc_fail ota_try ota_bad
- Versi OTA yang pernah gagal (ota_bad) tidak diunduh lagi: perbaikan wajib bernomor versi baru
## Upgrade
- Unggah .ino.bin + MD5 ke server dengan version persis 2.3.7. Verifikasi serial: [FW] FWVER:2.3.7;
## Rollback
- Otomatis bila image baru gagal sebelum RC522 OK. Armada: rilis versi lebih tinggi berisi kode lama. Per unit: flash USB.
