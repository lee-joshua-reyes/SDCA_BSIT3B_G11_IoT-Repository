#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <time.h>


// --------- WIFI ----------
const char* ssids[] = {
  "GlobeAtHome_5DD78_2.4",
  "GlobeAtHome_1D45D_2.4"
};

const char* passwords[] = {
  "025C300D",
  "zUCdG5Vb"
};

const int wifiCount = 2;

// --------- SERVER ----------
const char* SERVER_IP = "192.168.254.108";           // ← CHANGE THIS to your server IP
const int SERVER_PORT = 3000;

// --------- DEVICE CONFIG ----------
const char* DEVICE_ID = "Computer-Laboratory-1"; // assign this scanner to a room

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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  WiFi.mode(WIFI_STA);

  for (int i = 0; i < wifiCount; i++) {

    Serial.print("\nTrying SSID: ");
    Serial.println(ssids[i]);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting to:");
    lcd.setCursor(0, 1);
    lcd.print(ssids[i]);

    WiFi.begin(ssids[i], passwords[i]);

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

      return; // ✅ stop once connected
    }

    Serial.println("\nFailed. Trying next...");
  }

  // ❌ If all WiFi failed
  Serial.println("\n✗ All WiFi FAILED!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi FAILED");
  lcd.setCursor(0, 1);
  lcd.print("Retry...");
  delay(3000);
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
  payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
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

StaticJsonDocument<2048> doc;   // 512 can be too small for your payload
DeserializationError error = deserializeJson(doc, response);

if (error) {
  Serial.println("JSON Parse Failed");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Parse Error");
  beepBuzzer(100, 3);
  http.end();
  return;
}

bool ok = doc["success"] | false;
String scanStatus = doc["scan"]["status"] | "unknown";
String analysisStatus = doc["analysis"]["status"] | "unknown";
String name = doc["user"]["full_name"] | "Unknown user";

lcd.clear();

// Fail fast if API itself says request failed
if (!ok) {
  lcd.setCursor(0, 0);
  lcd.print("API Failed");
  beepBuzzer(100, 3);
}
// Primary access decision should use scan.status
else if (scanStatus == "success") {
  // Optional sub-state from analysis.status
  if (analysisStatus == "late") {
    lcd.setCursor(0, 0);
    lcd.print("Late Check-in");
    lcd.setCursor(0, 1);
    lcd.print(name.substring(0, 16));
    beepBuzzer(100, 2);
  } else if (analysisStatus == "outside_schedule") {
    lcd.setCursor(0, 0);
    lcd.print("Outside Time");
    lcd.setCursor(0, 1);
    lcd.print(name.substring(0, 16));
    beepBuzzer(100, 2);
  } else if (analysisStatus == "no_schedule" || analysisStatus == "unauthorized_access") {
    lcd.setCursor(0, 0);
    lcd.print("No Schedule");
    lcd.setCursor(0, 1);
    lcd.print(name.substring(0, 16));
    beepBuzzer(100, 2);
  } else {
    // on_time, early, anomaly, duplicate_scan, etc.
    lcd.setCursor(0, 0);
    lcd.print("Access OK");
    lcd.setCursor(0, 1);
    lcd.print(name.substring(0, 16));
    beepBuzzer(200, 1);
  }
}
else if (scanStatus == "failed") {
  if (analysisStatus == "wrong_room") {
    lcd.setCursor(0, 0);
    lcd.print("Wrong Room!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Scan Failed");
  }
  lcd.setCursor(0, 1);
  lcd.print(name.substring(0, 16));
  beepBuzzer(100, 3);
}
else if (scanStatus == "not_registered") {
  lcd.setCursor(0, 0);
  lcd.print("Card Unknown");
  beepBuzzer(100, 3);
}
else {
  lcd.setCursor(0, 0);
  lcd.print("Unknown Resp");
  beepBuzzer(100, 2);
}

  http.end();

  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Computer Lab 1");
}
else {
  Serial.print("✗ HTTP Error: ");
  Serial.println(http.errorToString(httpCode));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Server Error");
  beepBuzzer(100, 3);

  http.end();
}
  // LCD feedback
 

  // Reset display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DomStax");
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
 
// --------- time fix ----------

bool syncRTCFromNTP() {
  // UTC+8 (PH), no DST
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("NTP sync failed");
    return false;
  }

  rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));

  Serial.println("RTC synced from NTP");
  return true;
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
  Serial.println("RTC lost power detected");
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

if (WiFi.status() == WL_CONNECTED) {
  if (!syncRTCFromNTP()) {
    Serial.println("Using RTC stored time");
  }
} else {
  Serial.println("No WiFi; using RTC stored time");
}

  // Initial display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Computer Lab 1");

  Serial.println("\n✓ Setup Complete - Ready to scan cards!\n");
}

// --------- UPDATE CLOCK ----------
void updateClock() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate >= 1000) {
    lastUpdate = currentMillis;

    DateTime now = rtc.now() - TimeSpan(0, 0, 2, 0);

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
      lcd.print("Computer Lab 1");

      // Stop reading
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  delay(100);
}
