# Prompt Kerja: hardening-2.3.6
- lanjutkan perbaiki kode saya pertahap dan mapan. berikan dalam bentuk full function dan potongan kode yang lengkap. misalkan ini diganti dengan ini , sisipkan ini sebelum baris ini
- berikan catatan untuk tahap yang sudah dikerjakan.

curl -o gtsr4.pem https://pki.goog/repo/certs/gtsr4.pem
openssl x509 -in gtsr4.pem -noout -subject -issuer -dates -fingerprint -sha256

curl -o isrgx1.pem https://letsencrypt.org/certs/isrgrootx1.pem
