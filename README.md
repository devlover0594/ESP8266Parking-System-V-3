# Sistem Parkir Pintar Dual-Gate Berbasis ESP8266 (Versi 3)

Sistem Parkir Pintar Dual-Gate berbasis ESP8266 ini memanfaatkan komunikasi nirkabel **ESP-NOW**, pemindai **NFC/RFID MFRC522**, layar **LCD 16x2 I2C**, modul audio **DFPlayer Mini**, dan sensor **IR Obstacle (KY-032)**.

Pada **Versi 3**, sistem dilengkapi dengan kemampuan menyimpan **banyak kendaraan sekaligus (Multi-UID)**, melakukan **perhitungan biaya parkir otomatis berdasarkan durasi waktu**, serta menampilkan informasi saldo dan transaksi pada layar LCD I2C.

---

## 🏗️ Arsitektur Sistem

```
+-----------------------------------------------------------------------+
|                            GATE MASUK                                 |
|                                                                       |
|  [Sensor IR] ----> [DFPlayer: 001.mp3 "Silahkan Tap"]                 |
|  [MFRC522]   ----> Ambil UID + Waktu Masuk (millis)                   |
|                    Saldo Awal = Rp 5.000                              |
|  [LCD 16x2]  ----> Tampilkan "Silahkan Masuk" & Saldo                 |
|  [Servo SG90]----> Buka Palang                                        |
|                                                                       |
|  [ESP-NOW]   ===================================> [Kirim Struct Data] |
+-----------------------------------------------------------------------+
                                                               |
                                                               v
+-----------------------------------------------------------------------+
|                            GATE KELUAR                                |
|                                                                       |
|  [ESP-NOW]   ----> Simpan Data ke Array 'activeVehicles[]'            |
|  [Sensor IR] ----> [DFPlayer: 003.mp3 "Silahkan Tap"]                 |
|  [MFRC522]   ----> Baca UID & Cari di Array                            |
|                                                                       |
|  Jika UID Cocok:                                                      |
|    - Hitung Durasi = (millis_keluar - millis_masuk)                   |
|    - Hitung Biaya  = Durasi * Rp100/detik (Min. Rp500)                |
|    - Sisa Saldo    = Saldo Awal (Rp5.000) - Biaya                      |
|    - [LCD 16x2]   ----> Tampilkan Biaya & Sisa Saldo                  |
|    - [DFPlayer]   ----> Play 004.mp3 ("Terima Kasih")                 |
|    - [Servo SG90] ----> Buka Palang                                   |
|    - Hapus UID dari Array 'activeVehicles[]'                          |
|                                                                       |
|  Jika UID Salah / Tidak Ditemukan:                                    |
|    - [LCD 16x2]   ----> Tampilkan "Kartu Salah!"                      |
|    - [DFPlayer]   ----> Play 005.mp3 ("Kartu Salah")                  |
|    - Palang Tetap Tertutup                                            |
+-----------------------------------------------------------------------+
```

---

## 🌟 Fitur Utama (Versi 3)

1. **Komunikasi Nirkabel Cepat (ESP-NOW):** Pengiriman data UID dan waktu masuk dari Gate Masuk ke Gate Keluar tanpa membutuhkan router WiFi/broker MQTT.
2. **Multi-UID Database (Array Dinamis):** Mampu menampung hingga 10 kendaraan aktif dalam area parkir secara bersamaan tanpa risiko data tertimpa.
3. **Pencatatan Waktu & Biaya Parkir:** Menggunakan pewaktuan internal (`millis()`) untuk menghitung durasi parkir dan tarif secara akurat (Simulasi: Rp100/detik, Tarif Minimal Rp500).
4. **Simulasi Saldo Otomatis:** Setiap kartu NFC/RFID yang terdeteksi di Gate Masuk otomatis diberikan saldo awal sebesar **Rp5.000**.
5. **Umpan Balik Visual (LCD 16x2 I2C):** Menampilkan petunjuk interaksi, biaya parkir, dan sisa saldo secara *real-time*.
6. **Panduan Suara Interaktif (DFPlayer Mini):** Memberikan instruksi suara otomatis di setiap tahap alur parkir.

---

## 🗂️ Struktur Data ESP-NOW

Data dikirimkan dari Gate Masuk ke Gate Keluar menggunakan struktur *C-struct*:

```cpp
typedef struct struct_message {
  byte uid[4];              // 4 Byte UID Kartu NFC/RFID
  unsigned long entryTime;  // Timestamp waktu masuk (milidetik)
  int initialBalance;       // Saldo awal simulasi (Rp 5.000)
} struct_message;
```

---

## 💻 Kode Program Utama

### 1. Program Gate Masuk (`Gate_Masuk_V3.ino`)

```cpp
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define SS_PIN D3
#define RST_PIN D4
#define IR_PIN D0
#define SERVO_PIN D8

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

SoftwareSerial mySoftwareSerial(D9, D10); // Pin RX/TX untuk DFPlayer Mini
DFRobotDFPlayerMini myDFPlayer;

typedef struct struct_message {
  byte uid[4];
  unsigned long entryTime;
  int initialBalance;
} struct_message;

struct_message outgoingData;
// Ganti dengan MAC Address ESP8266 Gate Keluar Anda
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 

bool waitingForCard = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  
  pinMode(IR_PIN, INPUT);
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Gate Masuk   ");
  lcd.setCursor(0, 1);
  lcd.print("   Siap...     ");

  mySoftwareSerial.begin(9600);
  myDFPlayer.begin(mySoftwareSerial);
  myDFPlayer.volume(25);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) return;
  esp_now_set_self_role(ESP_NOW_ROLE_COMMS);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMMS, 1, NULL, 0);
}

void loop() {
  int irState = digitalRead(IR_PIN);

  // Sensor IR Mendeteksi Kendaraan
  if (irState == LOW && !waitingForCard) {
    myDFPlayer.play(1); // 001.mp3: "Silahkan Tap Kartu"
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Selamat Datang!");
    lcd.setCursor(0, 1);
    lcd.print("Tap Kartu Anda");
    waitingForCard = true;
    delay(2000);
  }

  // Menunggu Kartu Di-Tap
  if (waitingForCard) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) {
        outgoingData.uid[i] = mfrc522.uid.uidByte[i];
      }
      outgoingData.entryTime = millis();
      outgoingData.initialBalance = 5000; // Saldo awal Rp5.000

      // Kirim Data via ESP-NOW
      esp_now_send(broadcastAddress, (uint8_t *) &outgoingData, sizeof(outgoingData));

      myDFPlayer.play(2); // 002.mp3: "Silahkan Masuk"
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Silahkan Masuk");
      lcd.setCursor(0, 1);
      lcd.print("Saldo: Rp5000");
      
      gateServo.write(90); // Buka Gate
      delay(4000);

      // Tunggu hingga kendaraan melewati sensor IR
      while (digitalRead(IR_PIN) == LOW) { delay(500); }
      gateServo.write(0);  // Tutup Gate
      waitingForCard = false;
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  Gate Masuk   ");
      lcd.setCursor(0, 1);
      lcd.print("   Siap...     ");

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }
}
```

---

### 2. Program Gate Keluar (`Gate_Keluar_V3.ino`)

```cpp
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define SS_PIN D3
#define RST_PIN D4
#define IR_PIN D0
#define SERVO_PIN D8

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

SoftwareSerial mySoftwareSerial(D9, D10);
DFRobotDFPlayerMini myDFPlayer;

typedef struct struct_message {
  byte uid[4];
  unsigned long entryTime;
  int initialBalance;
} struct_message;

#define MAX_PARKING 10
struct_message activeVehicles[MAX_PARKING];
int vehicleCount = 0;

bool waitingForExitTap = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  
  pinMode(IR_PIN, INPUT);
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Gate Keluar  ");
  lcd.setCursor(0, 1);
  lcd.print("   Siap...     ");

  mySoftwareSerial.begin(9600);
  myDFPlayer.begin(mySoftwareSerial);
  myDFPlayer.volume(25);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) return;
  esp_now_set_self_role(ESP_NOW_ROLE_COMMS);
  esp_now_register_recv_cb(OnDataRecv);
}

// Callback Terima Data ESP-NOW
void OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len) {
  struct_message receivedData;
  memcpy(&receivedData, incomingDataBytes, sizeof(receivedData));

  if (vehicleCount < MAX_PARKING) {
    activeVehicles[vehicleCount] = receivedData;
    vehicleCount++;
    Serial.println("Kendaraan Baru Terdaftar!");
  }
}

void loop() {
  int irState = digitalRead(IR_PIN);

  if (irState == LOW && vehicleCount > 0 && !waitingForExitTap) {
    myDFPlayer.play(3); // 003.mp3: "Silahkan Tap Kartu"
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Proses Keluar");
    lcd.setCursor(0, 1);
    lcd.print("Tap Kartu Anda");
    waitingForExitTap = true;
    delay(2000);
  }

  if (waitingForExitTap) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      int foundIndex = -1;

      // Pencarian UID pada array aktif
      for (int i = 0; i < vehicleCount; i++) {
        bool match = true;
        for (byte j = 0; j < 4; j++) {
          if (mfrc522.uid.uidByte[j] != activeVehicles[i].uid[j]) {
            match = false;
            break;
          }
        }
        if (match) {
          foundIndex = i;
          break;
        }
      }

      if (foundIndex != -1) {
        // UID Ditemukan: Kalkulasi Waktu & Biaya
        unsigned long exitTime = millis();
        unsigned long durationSeconds = (exitTime - activeVehicles[foundIndex].entryTime) / 1000;
        
        int parkingFee = durationSeconds * 100; // Tarif Rp100/detik
        if (parkingFee < 500) parkingFee = 500; // Minimal Rp500
        
        int remainingBalance = activeVehicles[foundIndex].initialBalance - parkingFee;

        myDFPlayer.play(4); // 004.mp3: "Terima kasih, selamat jalan"
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Biaya: Rp");
        lcd.print(parkingFee);
        lcd.setCursor(0, 1);
        lcd.print("Sisa: Rp");
        lcd.print(remainingBalance);

        gateServo.write(90); // Buka Gate
        delay(5000);

        while (digitalRead(IR_PIN) == LOW) { delay(500); }
        gateServo.write(0);  // Tutup Gate

        // Hapus UID dari array (Shift Element)
        for (int i = foundIndex; i < vehicleCount - 1; i++) {
          activeVehicles[i] = activeVehicles[i + 1];
        }
        vehicleCount--;
        waitingForExitTap = false;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  Gate Keluar  ");
        lcd.setCursor(0, 1);
        lcd.print("   Siap...     ");

      } else {
        // UID Tidak Terdaftar / Salah Kartu
        myDFPlayer.play(5); // 005.mp3: "Kartu yang anda tap salah"
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  Kartu Salah! ");
        lcd.setCursor(0, 1);
        lcd.print(" Akses Ditolak ");
        delay(3000);
        waitingForExitTap = false;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  Gate Keluar  ");
        lcd.setCursor(0, 1);
        lcd.print("   Siap...     ");
      }

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }
}
```