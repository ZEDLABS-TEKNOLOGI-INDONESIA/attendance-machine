---
name: firmware-review
description: Review perubahan attendance-machine.ino (firmware ESP32-C3 presensi) untuk race condition, mutex SD/Display, watchdog, heap, dan kontrak API. Gunakan saat user minta review, "cek sebelum upload", atau setelah mengubah task/sync/SD.
---

# Firmware Review

Baca CLAUDE.md dulu. Periksa:
1. Mutex: setiap acquireSD() punya releaseSD() di SEMUA jalur (termasuk early return). selectSD()/deselectSD() berpasangan.
2. Task: tidak ada delay() panjang; ada esp_task_wdt_reset() di loop panjang; stack tidak melebihi (buffer besar di stack OfflineRecord[25], uint8_t[1024]).
3. Memori: String dan DynamicJsonDocument di jalur panas, fragmentasi heap.
4. Pin: tidak ada konflik SPI/I2C/BOOT; RC522 SS tinggi saat SD aktif.
5. Kontrak API: lihat bagian "Kontrak dengan backend" di CLAUDE.md.
6. Offline-first: perubahan tidak boleh membuat data antrian hilang (file dihapus hanya setelah respons server terkonfirmasi).
7. Input dari server divalidasi (rentang jam, interval minimum).

Output: tabel temuan (Kritis / Saran), nomor fungsi/baris, perbaikan konkret.
