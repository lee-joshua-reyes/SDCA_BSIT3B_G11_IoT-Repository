#include <SPI.h>
#include <MFRC522.h>

// ------------------- PIN DEFINITIONS -------------------
#define SS_PIN 21       // MFRC522 SDA
#define RST_PIN 22      // MFRC522 RST
#define BUZZER_PIN 25   // Active buzzer

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();               // Initialize SPI bus
  mfrc522.PCD_Init();        // Initialize MFRC522
  Serial.println("MFRC522 initialized. Scan a card...");

  // ------------------- BUZZER SETUP -------------------
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // buzzer off initially
}

void loop() {
  // Check if a new card is present
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    // ------------------- PRINT UID TO SERIAL -------------------
    Serial.print("UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // ------------------- BUZZER BEEP -------------------
    digitalWrite(BUZZER_PIN, HIGH);  // buzzer ON
    delay(200);                       // beep duration
    digitalWrite(BUZZER_PIN, LOW);   // buzzer OFF

    // ------------------- HALT CARD -------------------
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    Serial.println("Remove the card to scan again...");
  }

  delay(50); // small delay for stability
}