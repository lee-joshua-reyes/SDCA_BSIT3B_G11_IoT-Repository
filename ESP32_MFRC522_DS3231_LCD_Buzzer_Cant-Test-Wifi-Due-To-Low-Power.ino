#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// --------- RFID SETTINGS ----------
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// --------- RTC ----------
RTC_DS3231 rtc;

// --------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --------- Buzzer ----------
#define BUZZER 25

unsigned long lastUpdate = 0; // For 1-second clock updates

// --------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();

  // Initialize RTC
  Wire.begin(21, 22);
  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, adjusting time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Initialize Buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // Static top line
  lcd.setCursor(0, 0);
  lcd.print("DomStaX ID Scan");
}

// Function to update live clock without clearing LCD
void updateClock() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= 1000) { // update every 1 second
    lastUpdate = currentMillis;

    DateTime now = rtc.now();
    lcd.setCursor(0, 1);
    char line[17];
    sprintf(line, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    lcd.print(line);
  }
}

// --------- LOOP ----------
void loop() {
  // Update live clock while waiting for a card
  updateClock();

  // Check for new RFID card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Read UID
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("Card UID: " + uid);

    // Get timestamp
    DateTime now = rtc.now();
    char timestamp[20];
    sprintf(timestamp, "%04d/%02d/%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    Serial.println("Timestamp: " + String(timestamp));

    // Display UID and timestamp
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ID: " + uid);
    lcd.setCursor(0, 1);
    lcd.print(timestamp);

    // Buzzer beep
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);

    // Wait 4 seconds before returning to live display
    delay(4000);

    // Reset screen (top line static)
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DomStaX ID Scan");

    // Immediately show live clock
    DateTime resetNow = rtc.now();
    lcd.setCursor(0, 1);
    char line[17];
    sprintf(line, "%02d:%02d:%02d", resetNow.hour(), resetNow.minute(), resetNow.second());
    lcd.print(line);
  }

  rfid.PICC_HaltA();
}