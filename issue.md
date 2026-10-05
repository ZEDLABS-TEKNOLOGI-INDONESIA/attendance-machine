# Issues
1. Konflik pin: PIN_BOOT dan PIN_OLED_SCL sama-sama 9. pinMode(PIN_BOOT, INPUT_PULLUP) dipanggil setelah Wire.begin, dan checkFactoryReset() membaca pin yang sama dengan clock I2C. Ini bisa memicu pembacaan LOW palsu atau mengganggu OLED. Pada C3 Super Mini, GPIO9 memang tombol BOOT, jadi sebaiknya pindahkan SCL ke pin lain.
2. appendFailedLogToSD: deselectSD() dan releaseSD() hanya ada di cabang else. Pada cabang normal mutex SD tidak dilepas, sehingga task lain akan menunggu sampai timeout. Ini bug serius.
3. fetchRemoteConfig: urlEncode dipanggil sebelum dideklarasikan (definisinya ada di bawah). Di .ino Arduino biasanya membuat prototipe otomatis, tapi lebih aman deklarasikan di atas. Nilai dari server juga tidak divalidasi (jam 0-23, interval minimum), padahal loadCredentials melakukannya.
4. chunkedSync: setelah sd.remove() pada file kosong, semuanya dilindungi mutex, tapi sd.exists() dipanggil tanpa selectSD(), jadi CS SD tidak diaktifkan di jalur itu.
5. downloadRfidDb: receivedEndMarker dideklarasikan ulang secara lokal dan menimpa variabel global (shadowing). Tidak berbahaya, tapi membingungkan.
6. WDT task loop: loop() sering return saat sync berjalan, aman. Tapi showProgress, delay() di setup() cukup lama dan hanya aman karena WDT 90 detik.
7. nvsBumpScanCount memakai time(nullptr) yang bisa salah jika NTP gagal (mencatat tanggal 1970).
8. WiFi.setInsecure(): TLS tidak memverifikasi sertifikat, sehingga rentan MITM. Pertimbangkan pin root CA Cloudflare.
9. README menyebut "Dim OLED" dan sleep; README versi vs. header sudah konsisten (2.3.3), tapi Updated: Agustus 2026 perlu dicek.
