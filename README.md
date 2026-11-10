# Kas XII PPLG - Cash Management

Sistem Manajemen Kas Kelas XII PPLG berbasis web interaktif dengan backend berkecepatan tinggi menggunakan bahasa pemrograman C dan antarmuka web modern.

## 🚀 Fitur Utama
- **Dashboard Kas:** Rekap total saldo kas, pemasukan, pengeluaran, donasi, dan statistik keuangan.
- **Pencatatan Pembayaran:** Manajemen pembayaran kas mingguan per siswa secara cepat dan interaktif.
- **Laporan & Rekapitulasi:** Visualisasi grafik keuangan, laporan pengeluaran, donasi, dan rekap detail per bulan.
- **High Performance C Backend:** Backend HTTP API ringan yang ditulis dalam bahasa C (`server/`).
- **Mode Read-Only Fallback:** Dukungan fallback data lokal jika dijalankan tanpa server C.
- **Security & Backup:** Hashing password (SHA-256), token otentikasi, enkripsi cadangan data berkala.

## 📁 Struktur Direktori
```
XIIPPLG/
├── asset/              # Logo dan aset visual
├── data/               # Penyimpanan basis data JSON (members, payments, expenses, config)
├── public/             # Frontend HTML, CSS, JavaScript (Vanilla JS & Modern Dashboard)
│   ├── admin.html      # Halaman login & administrasi
│   ├── dashboard.html  # Antarmuka utama dashboard kas
│   ├── css/            # Stylesheet responsif
│   └── js/             # Modul logika aplikasi & visualisasi
├── server/             # Backend server ditulis dalam C (Makefile, API router, auth, database)
├── run.sh              # Script otomatis kompilasi dan runner server
└── README.md
```

## 🛠️ Cara Menjalankan

### Persyaratan
- GCC Compiler & Make
- Linux / WSL / POSIX Environment

### Menjalankan Server
Jalankan runner script:
```bash
chmod +x run.sh
./run.sh
```
Aplikasi akan dikompilasi secara otomatis dan server berjalan di port `8080`:
```
http://localhost:8080/dashboard.html
```

