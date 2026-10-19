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
