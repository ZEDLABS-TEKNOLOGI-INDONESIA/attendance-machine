---
name: debug-device
description: Diagnosis masalah device presensi dari log Serial/gejala: WiFi putus-putus, WDT reset, download RFID DB gagal, remote config tidak ter-apply, SD tidak terbaca, dashboard scan 0. Gunakan saat user melaporkan perilaku aneh device.
---

# Debug Device

Minta: gejala, log Serial (115200) termasuk reset reason, versi firmware, kondisi daya (baterai/USB).
Urutan cek:
- WiFi disconnect meski RSSI kuat: brownout TX 19.5 dBm, WIFI_PS_MAX_MODEM, setAutoReconnect tumpang tindih. Tambah WiFi.onEvent untuk reason code; ubah satu variabel per uji.
- Reset mendadak: WDT (task mana tidak reset), mutex SD tidak dilepas, heap rendah.
- "UNDUH TERPUTUS": cek backend mengirim baris END; Cloudflare menghapus Content-Length.
- Remote config: device_id persis sama dengan di panel, urlEncode, di luar jam sleep, tunggu 10 menit.
- SD: kontensi SPI dengan RC522, CS pin, reinit lewat checkSDHealth (30 detik).
- Dashboard scan 0: kolom device_id di tabel presensi NULL (sisi Laravel).
Output: hipotesis berurutan, cara membuktikan masing-masing, perbaikan.
