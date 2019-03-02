
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <SoftwareSerial.h>

// Serial feedback configuration
#define DEBUG_PRINT true         // By default the script doesn't print useless information to the console
#define DELAY_SEND 100            // A delay used in communication with the ESP8266 module (in miliseconds)
#define PC_BOARD_BAUD 9600        // A baud used for a serial communication between PC and a microcontroller
#define BOARD_ESP_BAUD 9600       // A baud used for a serial communication between a microcontroller and ESP8266 module **Usualy 9600 BAUD**

// Pin number assignment
#define PN532_IRQ   (5)           // Interrupt pin    (NFC)
#define PN532_RESET (8)           // Reset pin        (NFC)
#define ESP8266_RX  3             // Reciever pin     (ESP8266)
#define ESP8266_TX  2             // Transmitter pin  (ESP8266)
#define LED_ERR     13            // Pin number of the LED used for relaying error messages (eg. PERMISSION DENIED)
#define LED_ACK     13            // Pin number of the LED used for relaying acknowledge messages (eg. CARD OK)

// Hard-coded master cards
#define MASTER_CARD_ID0 "6a323b0f"
#define MASTER_CARD_ID1 "6ab0650f"
#define MASTER_CARD_ID2 "6a7e400f"
//#define MASTER_CARD_ID3 "6aec940f"
#define MASTER_CARD_ID3 "brezvezenekiahsdjhasjhdsdbfhbchbhbeHAHAHA!"


// Init NFC & ESP8266 modules
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
SoftwareSerial ESP8266Serial (ESP8266_RX, ESP8266_TX);

// Other attribute variables
String MAC;
String currentUID;

void setup() {

  // Clear the tmp UID value (prepare it) and lock the machine (since there is no-one using it)
  currentUID = "";
  lockMachine();

  // Init the interrupt pin of the NFC module
  pinMode(PN532_IRQ, INPUT);

  // Init the error and acknowledgement LED pins
  pinMode(LED_ERR, OUTPUT);
  pinMode(LED_ACK, OUTPUT);

  // Start the required modules and wait a bit
  Serial.begin(PC_BOARD_BAUD);          // A serial communication (PC - device)
  ESP8266Serial.begin(BOARD_ESP_BAUD);  // A serial communication (device - ESP8266 module)
  nfc.begin();                          // NFC module
  delay(500);

  // "Scan" for a NFC module (If not found -> stop)
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

  // Configure the NFC module
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  
  // Read the MAC address of the WiFi module
  getMAC();

  Serial.println("\nWaiting for a RFID card...");
}

void loop() {
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
        Serial.println();
        success = false;
      }

      // Convert the UID into a string (from a array of ints)
      String uidStr = "";
      char uidPart[2];
      for (uint8_t i = 0; i < uidLength; i++) {
        sprintf(uidPart, "%.2x", uid[i]);
        uidStr += String(uidPart);
      }
      Serial.print("Recieved a card with ID: ");
      Serial.println(uidStr);

      //uidStr = MASTER_CARD_ID2;
      
      // If the previous user didnt log out of this machine yet (and a new user tried to log on), then discard this card query
      if (!currentUID.length() == 0 && !currentUID.equals(uidStr) ) {
        Serial.println("**ERROR: The machine was already unlocked by another user!");
        signalErr();
      } else {
        Serial.println("Checking ID for a permission...");
        
        // Check if the card is one of the master cards - if not, try to authenticate it
        if (uidStr.equals(MASTER_CARD_ID0) || uidStr.equals(MASTER_CARD_ID1) || uidStr.equals(MASTER_CARD_ID2) || uidStr.equals(MASTER_CARD_ID3)) {
          success = true;
          Serial.println("All permissions granted - Master card UID detected");
        } else {
          // Authenticate the card on the server
          success = authenticateCard(uidStr, currentUID.length() == 0);
          //success = false;
        }

        if (success) {
          Serial.println("Card successfully authenticated!");
          
          // If the machine is free (unlock it)
          if (currentUID.length() == 0) {
            Serial.println("#### Unlocking the machine");
            unlockMachine();
            signalAck();
            currentUID = uidStr;
          }
          // If the user is still logged on, log him off and lock the machine
          else {
            Serial.println("#### Locking the machine");
            lockMachine();
            currentUID = "";
          }
        } else {
          Serial.println("Card authentication failed!");
        }
      }
      delay(3000);
    } else {
      // PN532 probably timed out waiting for a card
      Serial.println("WARNING: NFC reader timed out waiting for a card!");
    }

    Serial.println("\nWaiting for a RFID card...");
  }
}

/***
   Signal with the LED ERROR that an error has occured 
*/
void signalErr() {

  int iFlashNumber = 7;
  for(; iFlashNumber > 0; iFlashNumber--) {
    digitalWrite(LED_ERR, HIGH);
    delay(200);
    digitalWrite(LED_ERR, LOW);
    delay(60);
  }
}

/***
   Signal with the LED ACK that the action was succesful
*/
void signalAck() {
  
  int iFlashNumber = 3;
  for(; iFlashNumber > 0; iFlashNumber--) {
    digitalWrite(LED_ACK, HIGH);
    delay(500);
    digitalWrite(LED_ACK, LOW);
    delay(100);
  }
}

/***
   Lock the machine on which the device is located
*/
void lockMachine() {
  // NEKI
}

/***
   Unlock a locked machine on which the device is located
*/
void unlockMachine() {
  // NEKI
}

/***
   A method that tries to acquire the MAC address until it is available to read
*/
inline void getMAC() {
  boolean status = false;
  
  while (!status) {
    delay(DELAY_SEND);

    // Flush away old characters in the buffer
    while (ESP8266Serial.available() > 0) {
      ESP8266Serial.readStringUntil('\n');
      delay(10);
    }
    delay(DELAY_SEND);

    ESP8266Serial.println("AT+CIPSTAMAC_CUR?");
    delay(DELAY_SEND);
    printResponseInto((String* )&MAC, false);
    status = MAC.substring(MAC.length() - 3 - 1, MAC.length() - 1).indexOf("OK") != -1;
  }
  MAC = MAC.substring(MAC.length() - 25 - 1, MAC.length() - 8 - 1);
}

/***
   Authenticate the card using a given UID, by sending a
   HTTP GET request.

   @param     UID - User card ID

   @return    permission
*/
boolean authenticateCard(String UID, boolean logon) {

  String action;
  if (logon) {
    action = "prizgi";
  } else {
    action = "ugasni";
  }
  
  // "Open" a link between a server and ESP8266 module
  ESP8266Serial.println("AT+CIPSTART=\"TCP\",\"lab.404.si\",80");
  delay(DELAY_SEND);
  /*
  if (DEBUG_PRINT) {
    printResponse();
  }
  */
  discardSerialBuffer(DEBUG_PRINT);

  // Generate a HTTP request using GET (or POST) and send it to the server
  //String cmd = generateHTTPRequestPOST(22222, "Prizgi");
  String cmd = generateHTTPRequestGET(UID, action);
  ESP8266Serial.println("AT+CIPSEND=" + String(cmd.length()));
  delay(DELAY_SEND);
  /*
  if (DEBUG_PRINT) {
    printResponse();
  }
  */
  discardSerialBuffer(DEBUG_PRINT);

  ESP8266Serial.print(cmd);
  delay(DELAY_SEND);

  // Print a recieved response into a variable & screen
  String response = "";
  Serial.println("***************************");
  while(true) {
    printResponseInto( (String*)&response, true );
    delay(100);
  }
  Serial.println("***************************");

  // Check if the card has permissions to use this device
  boolean status = checkPermission( (String*)&response );
  return status;
}

/***
   Print a response from the server into a serial communication (PC - device)
*/
void printResponse() {
  while (ESP8266Serial.available()) {
    //Serial.print("***");
    Serial.println(ESP8266Serial.readStringUntil('\n'));
  }
}

/***
   Print a response from the server into a given variable (and optionaly into a serial communication (PC - device))

   @param       buffer - A string pointer to a given HTTP response
   @param       printToConsole - Block/open a console from/for writing
*/
void printResponseInto(String *buffer, boolean printToConsole) {
  while (ESP8266Serial.available()) {
    *buffer += ESP8266Serial.readStringUntil('\n') + '\n';
  }

  if ( printToConsole ) {
    Serial.println(*buffer);
  }
}

/***
   Discard current content stored in the Serial communication between ESP8266 module and a microcontroller

   @param       printToConsole - Block/open a console from/for writing
*/
void discardSerialBuffer(boolean printToConsole) {
  String buffer = "";
  while (ESP8266Serial.available()) {
    buffer += ESP8266Serial.readStringUntil('\n') + '\n';
  }

  if ( printToConsole ) {
    Serial.println(buffer);
  }
}

/***
   Check the permission attribute in a given HTTP response and return a value of the permission

   @param       buffer - A string pointer to a given HTTP response

   @return      permission
*/
boolean checkPermission( String *buffer ) {

  String content = *buffer;
  int contentLength = content.length();
  if (contentLength < 19)
    return false;

  int lastLineIdx = content.lastIndexOf('\n', contentLength - 10);
  int permissionIdx = content.indexOf("permission", lastLineIdx);

  // If the substring "permission" does not exist in the string
  if (permissionIdx == -1)
    return false;

  int permissionValueIdx = content.substring(permissionIdx + 12, permissionIdx + 17).indexOf("true");
  if (permissionValueIdx == -1)
    return false;
  return true;
}

/***
   Generate a HTTP GET request from a given UID and action

   @param     action - can be "Prizgi" or "Ugasni" depending on a state the machine is in
   @param     cardUID - a UID read from the given card
*/
String generateHTTPRequestGET(String cardUID, String action) {

  cardUID = "6aec940f";
  MAC = "9199191";
  action = "ugasni";
  String request = "GET /api/v1/status/?format=json&id_kartice=" + cardUID + "&id_naprave=" + MAC + "&stanje=" + action + " HTTP/1.1\r\n";
  request += "Host: lab.404.si\r\n";
  request += "Connection: close\r\n\r\n";
  Serial.println("$$$$$$$");
  Serial.println(request);
  Serial.println("$$$$$$$");
  return request;
}

/***
          ######NOT COMPLETLY OPERATIONAL######
   Generate a HTTP POST request from a given UID and action

   @param     action - can be "Prizgi" or "Ugasni" depending on a state the machine is in
   @param     cardUID - a UID read from the given card
*/
String generateHTTPRequestPOST(int cardUID, String action) {

  String postContent = "format=json&id_kartice=" + String(cardUID) + "&id_naprave=" + MAC + "stanje=" + action;
  String cmd = "POST /api/v1/worklog/ HTTP/1.1\r\n";
  cmd += "Host: lab.404.si\r\n";
  cmd += "User-Agent: ESP8266\r\n";
  cmd += "Content-Length: ";
  cmd += postContent.length();
  cmd += "\r\nContent-Type: application/x-www-form-urlencoded\r\n";
  cmd += "Connection: close\r\n\r\n";
  cmd += postContent;

  return cmd;
}
