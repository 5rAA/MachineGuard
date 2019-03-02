
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>


#define DEBUG_PRINT false
#define PN532_IRQ   (5)   //16
#define PN532_RESET (8)  // Not connected by default on the NFC Shield

#define MASTER_CARD_ID0 "6a323b0f"
#define MASTER_CARD_ID1 "6ab0650f"
#define MASTER_CARD_ID2 "6a7e400f"


// Or use this line for a breakout or shield with an I2C connection:
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

int RE1 = 7;
int RE2 = 6;

void setup(void) {
  
  Serial.begin(115200);
  pinMode(RE1, OUTPUT);
  pinMode(RE2, OUTPUT);
  pinMode(PN532_IRQ, INPUT);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("ERROR: Didn't find any PN53x board");
    while (1); // halt
  }

  if (DEBUG_PRINT) {
    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
  }

  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();  // configure board to read RFID tags
  Serial.println("Waiting for a RFID card...");

}

void loop(void) {
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  if (digitalRead(PN532_IRQ) == 1) {
    // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
    // 'uid' will be populated with the UID, and uidLength will indicate
    // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

    if (success) {
      if (DEBUG_PRINT) {
        Serial.println("Found a card!");
        Serial.print("UID Length: ");
        Serial.print(uidLength, DEC);
        Serial.println(" bytes");
        Serial.print("UID Value: ");

        for (uint8_t i = 0; i < uidLength; i++) {
          Serial.print(uid[i]);
        }
      }

      String uidStr = "";
      char uidPart[2];
      for (uint8_t i = 0; i < uidLength; i++) {
        sprintf(uidPart, "%.2x", uid[i]);
        uidStr += String(uidPart);
      }
      Serial.print("Recieved a card with ID: ");
      Serial.println(uidStr);
      Serial.println("Checking ID for a permission...");

      if (uidStr.equals(MASTER_CARD_ID0) || uidStr.equals(MASTER_CARD_ID1) || uidStr.equals(MASTER_CARD_ID2)) {
        success = true;
        Serial.print("All permissions granted - Master card UID detected");
      } else {

        // Check with the servers (mišljenjem)
        success = false;
      }

      Serial.println(success);

      if (success) {
        digitalWrite(RE2, !digitalRead(RE2));
      }
      delay(1000);
    } else {
      // PN532 probably timed out waiting for a card
      Serial.println("WARNING: NFC reader timed out waiting for a card!");
    }
  }
  digitalWrite(RE1, !digitalRead(RE1));
}
