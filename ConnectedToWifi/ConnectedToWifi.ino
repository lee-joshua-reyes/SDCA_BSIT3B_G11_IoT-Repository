#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// --------- WIFI ----------
const char* ssid = "GlobeAtHome_1D45D_2.4";
const char* password = "zUCdG5Vb";

// --------- SERVER ----------
const char* SERVER_IP = "192.168.254.174";           // ← CHANGE THIS to your server IP
const int SERVER_PORT = 3000;

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

unsigned long lastUpdate = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // Check WiFi every 10 seconds

// --------- WIFI CONNECT ----------
void connectWiFi() {
  Serial.println("\n=== Connecting to WiFi ===");
  Serial.print("SSID: ");
  Serial.println(ssid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().c_str());
    delay(2000);
  } else {
    Serial.println("\n✗ WiFi Connection FAILED!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED");
    lcd.setCursor(0, 1);
    lcd.print("Retry...");
    delay(3000);
  }
}

// --------- CHECK WIFI CONNECTION ----------
void checkWiFiConnection() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = currentMillis;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Reconnecting...");
      connectWiFi();
    } else {
      Serial.println("✓ WiFi OK");
    }
  }
}

// --------- SEND RFID SCAN ----------
void sendRFIDScan(String uid) {
  Serial.println("\n=== Sending RFID Scan ===");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi not connected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Offline");
    beepBuzzer(100, 3);
    delay(2000);
    return;
  }

  HTTPClient http;

  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/iot/scans";

  Serial.print("URL: ");
  Serial.println(url);

  // Build JSON payload safely
  String payload = "{";
  payload += "\"deviceId\":\"esp32-main-gate\",";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"status\":\"success\"";
  payload += "}";

  Serial.print("Payload: ");
  Serial.println(payload);

  http.setTimeout(5000); // 5 second timeout
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  Serial.print("HTTP Response: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("Server Response: ");
    Serial.println(response);

    if (httpCode == 200) {
      Serial.println("✓ Scan sent successfully!");
      beepBuzzer(200, 1); // Success
    } else {
      Serial.print("⚠ Unexpected status code: ");
      Serial.println(httpCode);
      beepBuzzer(100, 2);
    }
  } else {
    Serial.print("✗ HTTP Error: ");
    Serial.println(http.errorToString(httpCode));
    beepBuzzer(100, 3); // Error
  }

  http.end();

  // LCD feedback
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Sent");
  lcd.setCursor(0, 1);
  if (httpCode == 200) {
    lcd.print("Success!");
  } else {
    lcd.print("Error: " + String(httpCode));
  }
  delay(2000);

  // Reset display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DomStaX ID Scan");
}

// --------- BUZZER ----------
void beepBuzzer(int duration, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(duration);
    digitalWrite(BUZZER, LOW);
    if (i < times - 1) delay(100);
  }
}

// --------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n╔════════════════════════════════════╗");
  Serial.println("║  ESP32 RFID Reader with HTTP API  ║");
  Serial.println("║         Version 2.1.0             ║");
  Serial.println("╚════════════════════════════════════╝\n");

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("✓ RFID Reader initialized");

  // Initialize RTC
  Wire.begin(21, 22);
  if (!rtc.begin()) {
    Serial.println("✗ RTC NOT FOUND - System halted");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("⚠ RTC lost power - Setting current time");
    // Use hardcoded date/time instead of macros
    rtc.adjust(DateTime(2026, 4, 20, 12, 0, 0));
  }
  Serial.println("✓ RTC initialized");

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 RFID");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  Serial.println("✓ LCD initialized");

  // Initialize Buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  Serial.println("✓ Buzzer initialized");

  // Startup beep
  beepBuzzer(100, 2);

  // Connect to WiFi
  connectWiFi();

  // Initial display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DomStaX ID Scan");

  Serial.println("\n✓ Setup Complete - Ready to scan cards!\n");
}

// --------- UPDATE CLOCK ----------
void updateClock() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate >= 1000) {
    lastUpdate = currentMillis;

    DateTime now = rtc.now();

    lcd.setCursor(0, 1);
    char line[17];
    sprintf(line, "%02d:%02d:%02d     ", now.hour(), now.minute(), now.second());
    lcd.print(line);
  }
}

// --------- MAIN LOOP ----------
void loop() {
  // Check WiFi connection periodically
  checkWiFiConnection();

  // Update clock display
  updateClock();

  // Check for new RFID card
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {

      String uid = "";

      // Convert UID to hex string
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      uid.toUpperCase();

      Serial.print("\n>>> CARD SCANNED: ");
      Serial.println(uid);

      // Get timestamp from RTC
      DateTime now = rtc.now();
      char timeStr[9];
      sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

      // Display on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("UID: ");
      lcd.print(uid.substring(0, 8));

      lcd.setCursor(0, 1);
      lcd.print(timeStr);

      Serial.print("Time: ");
      Serial.println(timeStr);

      // Success beep
      beepBuzzer(200, 1);

      // Send to server
      sendRFIDScan(uid);

      delay(3000);

      // Reset display
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DomStaX ID Scan");

      // Stop reading
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  delay(100);
}