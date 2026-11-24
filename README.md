# Kas XII PPLG

Dashboard kas kelas XII PPLG, SMKN 2 Kuta Selatan. Frontend-nya HTML, CSS dan JavaScript biasa, sedangkan backend-nya server HTTP kecil yang saya tulis sendiri dalam C, tanpa framework.

**Versi online (hanya baca):** [zynxx24.github.io/XII-PPLG---CASH-EXPORTED](https://zynxx24.github.io/XII-PPLG---CASH-EXPORTED/). Versi itu adalah ekspor statis dari repo ini, dengan data yang disalin dari folder `data/`.

![Dashboard kas XII PPLG](https://raw.githubusercontent.com/zynxx24/XII-PPLG---CASH-EXPORTED/main/docs/screenshot-desktop.webp)

## Fitur

- Dashboard: saldo, pemasukan, pengeluaran, donasi, tunggakan, dan grafik perkembangan saldo.
- Pembayaran kas mingguan per siswa (Rp5.000 per minggu), rekap per bulan, dan laporan.
- Catatan pengeluaran dan donasi.
- Login admin untuk mengubah data. Tanpa login, halaman hanya bisa dibaca.
- Kalau server C tidak jalan, frontend membaca data statis sehingga halaman tetap tampil dalam mode baca saja.

## Menjalankan

Butuh `gcc` dan `make` di Linux, WSL, atau sistem POSIX lain.

```bash
git clone https://github.com/zynxx24/Kas-XIIPPLG---cash-management.git
cd Kas-XIIPPLG---cash-management

cp server/.ENV.example server/.ENV   # lalu isi ADMIN_PASSWORD dan BACKUP_KEY
chmod +x run.sh

./run.sh local    # hanya di komputer ini
./run.sh          # plus Cloudflare Tunnel, mencetak URL publik trycloudflare.com
```

Server berjalan di `http://localhost:8080`. Mode default (`./run.sh` tanpa argumen) membuka tunnel publik dan akan memasang `cloudflared` kalau belum ada, jadi pakai `local` kalau hanya ingin mencoba.

## API

Semua endpoint ada di bawah `/api`:

| Endpoint | Isi |
|---|---|
| `POST /api/login`, `POST /api/logout` | Sesi admin atau anggota, memakai token |
| `GET /api/dashboard` | Ringkasan saldo dan statistik |
| `/api/members`, `/api/payments` | Data siswa dan pembayaran |
| `/api/expenses`, `/api/donations` | Pengeluaran dan donasi |
| `/api/config` | Nominal kas dan pengaturan |
| `/api/security` | Statistik permintaan dan pemakaian CPU |

## Keamanan

- Kredensial admin dibaca dari `server/.ENV`, yang tidak ikut di-commit. Server hanya menyimpan dan membandingkan hash SHA-256 password-nya.
- Kalau `server/.ENV` tidak ada, server memakai password bawaan. Karena itu `./run.sh` menolak membuka tunnel publik sebelum file itu dibuat.
- Setiap IP dibatasi jumlah permintaannya, dan server memantau pemakaian CPU.
- Backup data di `data/backups/` dienkripsi dengan `BACKUP_KEY` dan tidak ikut di-commit.

## Struktur

```
data/      basis data JSON (members, payments, expenses, donations, config)
public/    frontend: admin.html, dashboard.html, css/, js/pages/
server/    server C: main, server, router, api, auth, database, security, sha256, cJSON
run.sh     kompilasi, jalankan server, dan buka tunnel
```
