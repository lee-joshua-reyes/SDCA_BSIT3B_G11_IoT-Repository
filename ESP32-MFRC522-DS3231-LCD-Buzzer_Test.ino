#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// RFID
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// RTC
RTC_DS3231 rtc;

// LCD (try 0x27 first, change to 0x3F if needed)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buzzer
#define BUZZER 25

void setup() {
  Serial.begin(115200);

  // RFID
  SPI.begin();
  rfid.PCD_Init();

  // I2C
  Wire.begin(21, 22);

  // RTC
  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // LCD
  lcd.init();
  lcd.backlight();

  // Buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // Startup screen
  lcd.setCursor(0, 0);
  lcd.print("DomStaX Attendance");
  lcd.setCursor(0, 1);
  lcd.print("Scan your card");
}

void loop() {
  DateTime now = rtc.now();

  // Display current time
  lcd.setCursor(0, 1);
  lcd.print("Time:");
  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());
  lcd.print(" ");

  // Wait for RFID card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Read UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  Serial.println("Card UID: " + uid);

  // Get time
  String timeNow = String(now.hour()) + ":" +
                   String(now.minute()) + ":" +
                   String(now.second());

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID:");
  lcd.print(uid);

  lcd.setCursor(0, 1);
  lcd.print(timeNow);

  // Buzzer beep
  digitalWrite(BUZZER, HIGH);
  delay(200);
  digitalWrite(BUZZER, LOW);

  delay(5000);

  // Reset screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan your card");
  
  rfid.PICC_HaltA();
}