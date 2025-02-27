#include <SPI.h>
#include <MFRC522.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define SS_PIN  5 //SDA
#define RST_PIN 4 //RST
#define RELAY_PIN  2 // Define the relay pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const char* ssid = "iPhone";
const char* password = "0951388070z";

void sendMacAddressAndUIDToAPI(const String& macAddress, const String& uid, bool uidFound);
void openDoor();

void sendMacAddressAndUIDToAPI(const String& macAddress, const String& uid, bool uidFound) {
  HTTPClient http;
  http.begin("http://172.20.10.3:5000/api/mac");
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"mac_address\": \"" + macAddress + "\", \"uid\": \"" + uid + "\", \"uid_found\": " + (uidFound ? "true" : "false") + "}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);

    if (!uidFound) {
      if (response.indexOf("No such UID found") != -1) {
        Serial.println("UID not found in database.");
      } else {
        Serial.print("UID found: ");
        Serial.println(response);
        
        // ตรวจสอบว่า response ไม่ใช่ error ก่อนบันทึกลงไฟล์
        if (response.indexOf("error") == -1) {
          File file = SPIFFS.open("/UIDs.csv", FILE_APPEND);
          if (file) {
            file.println(uid + "," + response);
            file.close();
          } else {
            Serial.println("Failed to open UIDs.csv for writing");
          }
          openDoor(); // เปิดประตูถ้า UID ได้รับการยืนยันจาก API
        }
      }
    }
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


void openDoor() {
  digitalWrite(RELAY_PIN, HIGH); // Turn on the relay
  delay(1000); // Keep the relay on for 1 second
  digitalWrite(RELAY_PIN, LOW); // Turn off the relay
}

void setup() {
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.begin(115200);  // Initialize serial communications
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  Serial.println("Scan a RFID card");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized.");
  pinMode(RELAY_PIN, OUTPUT); // Initialize the relay pin as an output
}

void loop() {
    // Look for new cards
    if (!mfrc522.PICC_IsNewCardPresent()) {
      return;
    }
  
    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
      return;
    }
  
    // Show UID on serial monitor
    String content = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
      content.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    content.toUpperCase();
    Serial.print("UID tag : ");
    Serial.println(content);
  
    // Check if the UID is known
    bool uidFound = false;
    File file = SPIFFS.open("/UIDs.csv");
    if (file) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        int commaIndex = line.indexOf(',');
        String uid = line.substring(0, commaIndex);
        String owner = line.substring(commaIndex + 1);
  
        if (content.equals(uid)) {
          Serial.print("Hello ");
          Serial.println(owner);
          Serial.println("");
          uidFound = true;
          String macAddress = WiFi.macAddress();
          sendMacAddressAndUIDToAPI(macAddress, content, true);
          openDoor(); // Open the door if the UID is found locally
          break;
        }
      }
      file.close();
    } else {
      Serial.println("Failed to open UIDs.csv");
    }

    if (!uidFound) {
      String macAddress = WiFi.macAddress();
      sendMacAddressAndUIDToAPI(macAddress, content, false);
    }
}



