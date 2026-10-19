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
