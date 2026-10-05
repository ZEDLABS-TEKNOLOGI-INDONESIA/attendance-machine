# Prompt Kerja: hardening-2.3.4

Aturan: satu prompt per sesi, `/clear` di antaranya. Review `git diff`, uji di satu device, baru commit.

## Status

| # | Tugas | Diff direview | Compile | Uji device | Commit |
|---|-------|:-:|:-:|:-:|:-:|
| 0 | Baseline review | [ ] | - | - | [ ] |
| 1 | Mutex SD bocor di appendFailedLogToSD | [ ] | [ ] | [ ] | [ ] |
| 2 | Audit mutex menyeluruh (laporan) | [ ] | - | - | - |
| 3 | Validasi remote config + prototipe urlEncode | [ ] | [ ] | [ ] | [ ] |
| 4 | selectSD/deselectSD di chunkedSync | [ ] | [ ] | [ ] | [ ] |
| 5 | Keputusan konflik GPIO9 | [ ] | [ ] | [ ] | [ ] |
| 6 | WiFi.onEvent disconnect reason | [ ] | [ ] | [ ] | [ ] |
| 7 | Kebersihan kecil (shadowing, tanggal 1970) | [ ] | [ ] | [ ] | [ ] |
| 8 | Rilis 2.3.4 | [ ] | [ ] | [ ] | [ ] |

## 0. Baseline
```
Baca CLAUDE.md dan issue.md. Jalankan /firmware-review pada attendance-machine.ino.
Jangan ubah file apa pun. Gabungkan temuanmu dengan issue.md, urutkan dari risiko
tertinggi, tandai bug nyata vs saran, lalu perbarui issue.md dengan nomor urut.
```

## 1. Mutex SD bocor
```
Kerjakan isu appendFailedLogToSD: deselectSD() dan releaseSD() hanya dipanggil
di cabang else, jalur normal tidak melepas mutex. Periksa SEMUA jalur keluar
fungsi, perbaiki seminimal mungkin, tampilkan diff, compile, laporkan ukuran
sketch. Jangan commit.
```

## 2. Audit mutex
```
Telusuri semua pemanggilan acquireSD() dan selectSD(). Buat tabel: fungsi,
jalur keluar, apakah release/deselect terpenuhi. Hanya laporan, jangan ubah kode.
```

## 3. Validasi remote config
```
Di fetchRemoteConfig, validasi nilai dari server: jam 0-23, interval minimal
5000 ms, seperti loadCredentials. Nilai invalid diabaikan per field. Pindahkan
deklarasi urlEncode ke atas file. Diff, compile.
```

## 4. Chip select chunkedSync
```
Di chunkedSync, sd.exists() dan sd.remove() dipanggil tanpa selectSD()/deselectSD().
Pastikan semua akses SD berpasangan dengan selectSD/deselectSD dan mutex.
Ubah seminimal mungkin, diff, compile.
```

## 5. Konflik GPIO9
```
PIN_BOOT dan PIN_OLED_SCL sama-sama GPIO9. Jelaskan dampak nyata pada
checkFactoryReset dan I2C OLED, beri 2-3 opsi beserta konsekuensi wiring.
Jangan ubah kode sebelum saya memilih.
```

## 6. Diagnosis WiFi
```
Tambahkan WiFi.onEvent yang mencetak disconnect reason code ke Serial (hanya
log, jangan ubah setSleep/TX power/autoReconnect). Compile, jelaskan cara
membaca hasilnya.
```

## 7. Kebersihan kecil
```
Perbaiki: (a) shadowing receivedEndMarker di downloadRfidDb, (b) scan counter
jangan tertulis tanggal 1970 saat waktu belum valid. Sarankan pesan commit
terpisah per perubahan. Diff dulu.
```

## 8. Rilis
```
Jalankan /release untuk 2.3.4. Ubah FIRMWARE_VERSION, header komentar, dan README
serentak. Jangan tag sebelum saya konfirmasi uji di device.
```
