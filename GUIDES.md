# Panduan Langkah demi Langkah Implementasi Sistem Parkir ESP8266 (Versi 3)

Dokumen ini berisi panduan teknis perakitan, konfigurasi I2C/SPI, pengunggahan kode, dan pengujian sistem parkir dual-gate Versi 3.

---

## Langkah 1: Persiapan Hardware dan Komponen

| Komponen | Gate Masuk | Gate Keluar | Total | Keterangan |
| :--- | :---: | :---: | :---: | :--- |
| ESP8266 NodeMCU / Wemos D1 Mini | 1 | 1 | 2 | Kontroler utama |
| Modul RFID MFRC522 | 1 | 1 | 2 | Pemindai NFC/RFID |
| Display LCD 16x2 + Backpack I2C | 1 | 1 | 2 | Output teks visual |
| Sensor Obstacle IR (KY-032) | 1 | 1 | 2 | Sensor deteksi kendaraan |
| Servo Motor SG90 / MG996R | 1 | 1 | 2 | Palang pintu otomatis |
| DFPlayer Mini + MicroSD | 1 | 1 | 2 | Pemutar modul suara |
| Speaker Kecil (8$\Omega$ / 0.5W–3W) | 1 | 1 | 2 | Output audio |
| Resistor 1k$\Omega$ | 1 | 1 | 2 | Proteksi RX DFPlayer |
| Catu Daya / Power Supply 5V (2A-3A) | 1 | 1 | 2 | Sumber daya external |

---

## Langkah 2: Pemetaan Pinout (Wiring Diagram Versi 3)

Karena penambahan LCD I2C pada Versi 3, terdapat penyesuaian pin SPI untuk MFRC522 agar tidak bentrok dengan bus I2C (SDA/SCL).

### 1. Display LCD 16x2 I2C (Bus I2C)
| Pin LCD I2C | Pin ESP8266 | Keterangan |
| :--- | :--- | :--- |
| **VCC** | 5V (VIN) | Daya LCD |
| **GND** | GND | Ground |
| **SDA** | **D2** (GPIO 4) | Serial Data I2C |
| **SCL** | **D1** (GPIO 5) | Serial Clock I2C |

### 2. Modul RFID-RC522 (SPI)
| Pin MFRC522 | Pin ESP8266 | Keterangan |
| :--- | :--- | :--- |
| **VCC** | 3.3V | *Wajib 3.3V! Jangan ke 5V!* |
| **RST** | **D4** (GPIO 2) | Reset Pin |
| **GND** | GND | Ground |
| **MISO** | **D6** (GPIO 12) | SPI MISO |
| **MOSI** | **D7** (GPIO 13) | SPI MOSI |
| **SCK** | **D5** (GPIO 14) | SPI Clock |
| **SDA (SS)** | **D3** (GPIO 0) | SPI Chip Select |

### 3. Servo SG90
| Pin Servo | Pin ESP8266 / Power Supply |
| :--- | :--- |
| **VCC (Merah)** | 5V External / VIN |
| **GND (Cokelat/Hitam)** | GND (Common Ground) |
| **Signal (Kuning/Oranye)** | **D8** (GPIO 15) |

### 4. Sensor Obstacle IR (KY-032)
| Pin KY-032 | Pin ESP8266 |
| :--- | :--- |
| **VCC** | 3.3V / 5V |
| **GND** | GND |
| **OUT** | **D0** (GPIO 16) |

### 5. DFPlayer Mini
| Pin DFPlayer | Connection |
| :--- | :--- |
| **VCC** | 5V |
| **GND** | GND |
| **RX** | Resistor 1k$\Omega$ $\rightarrow$ **D10** (GPIO 10 / SD3) / SoftwareSerial |
| **TX** | **D9** (GPIO 9 / SD2) / SoftwareSerial |
| **SPK_1 / SPK_2** | Terhubung langsung ke Speaker |

---

## Langkah 3: Format MicroSD dan Berkas Audio

1. Format kartu MicroSD ke sistem berkas **FAT32**.
2. Buat direktori bernama `mp3` pada MicroSD.
3. Simpan file suara dengan format berikut:

```text
/mp3/
  ├── 001.mp3  --> "Silahkan tap kartu"
  ├── 002.mp3  --> "Silahkan masuk"
  ├── 003.mp3  --> "Silahkan tap kartu"
  ├── 004.mp3  --> "Terima kasih, selamat jalan"
  └── 005.mp3  --> "Kartu yang anda tap salah"
```

---

## Langkah 4: Pencarian MAC Address & Konfigurasi ESP-NOW

1. Unggah skrip berikut ke ESP8266 **Gate Keluar**:

```cpp
#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("MAC Address Gate Keluar: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}
```

2. Buka Serial Monitor (baud rate `115200`).
3. Catat alamat MAC yang muncul (contoh: `A4:CF:12:89:56:78`).
4. Buka kode `Gate_Masuk_V3.ino`, lalu sesuaikan variabel `broadcastAddress[]`:

```cpp
uint8_t broadcastAddress[] = {0xA4, 0xCF, 0x12, 0x89, 0x56, 0x78};
```

---

## Langkah 5: Pengujian Fitur Versi 3

### Skenario 1: Masuk Dua Kendaraan Berbeda
1. **Kendaraan A (Kartu A):**
   * IR Masuk aktif $\rightarrow$ Audio `001.mp3` $\rightarrow$ LCD: "Tap Kartu Anda".
   * Tap Kartu A $\rightarrow$ Audio `002.mp3` $\rightarrow$ LCD: "Saldo: Rp5000" $\rightarrow$ Palang terbuka.
   * Data Kartu A & Timestamp masuk tersimpan di Gate Keluar.
2. **Kendaraan B (Kartu B):**
   * IR Masuk aktif $\rightarrow$ Tap Kartu B.
   * Data Kartu B & Timestamp masuk tersimpan sebagai item ke-2 di Gate Keluar.

### Skenario 2: Keluar dan Perhitungan Biaya Parkir
1. **Kartu A Keluar Dulu:**
   * IR Keluar aktif $\rightarrow$ Tap Kartu A.
   * System mencocokkan Kartu A $\rightarrow$ Menghitung selisih waktu (`millis()`).
   * LCD menampilkan **Biaya Parkir** & **Sisa Saldo** (Rp5.000 - Biaya).
   * Audio `004.mp3` berbunyi $\rightarrow$ Palang Terbuka $\rightarrow$ Data Kartu A dihapus dari memori.
2. **Kartu B Keluar:**
   * Tap Kartu B $\rightarrow$ Biaya dihitung berdasarkan waktu masuk Kartu B tersendiri.

### Skenario 3: Kartu Tidak Terdaftar / Salah
1. Tap Kartu C (kartu yang belum pernah lewat Gate Masuk).
2. LCD menampilkan: **"Kartu Salah! Akses Ditolak"**.
3. Audio `005.mp3` berbunyi $\rightarrow$ Palang Pintu **tetap tertutup**.