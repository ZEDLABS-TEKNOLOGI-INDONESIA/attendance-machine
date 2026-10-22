#pragma once
// Deklarasi maju. Dulu dibuat otomatis oleh arduino-cli untuk fungsi di .ino;
// sekarang kode ada di header, jadi fungsi yang dipakai sebelum didefinisikan
// harus dideklarasikan manual di sini.
bool getTimeWithFallback(struct tm *ti);
