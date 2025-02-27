#include <SPI.h>
#include <MFRC522.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

#define SS_PIN  5 //SDA
#define RST_PIN 4 //RST
#define RELAY_PIN  2 // Define the relay pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const char* ssid = "iPhone";
const char* password = "0951388070z";
const char* hostname = "ESP32-EN4411";  // Set your desired hostname

AsyncWebServer server(80);

void sendMacAddressAndUIDToAPI(const String& macAddress, const String& uid, bool uidFound);
void openDoor();
void checkWiFiConnection();

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

        // Parse the owner UID from the response
        int ownerUidStart = response.indexOf("\"owner_uid\":\"") + 13;
        int ownerUidEnd = response.indexOf("\"", ownerUidStart);
        String ownerUid = response.substring(ownerUidStart, ownerUidEnd);

        // ตรวจสอบว่า response ไม่ใช่ error ก่อนบันทึกลงไฟล์
        if (response.indexOf("error") == -1) {
          File file = SPIFFS.open("/UIDs.csv", FILE_APPEND);
          if (file) {
            file.println(uid + "," + ownerUid);
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

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
      delay(1000);
      Serial.println("Reconnecting to WiFi...");
      retryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi");
    } else {
      Serial.println("Failed to reconnect to WiFi");
    }
  }
}

void setup() {
  Serial.begin(115200);  // Initialize serial communications
  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.setHostname(hostname);  // Set the hostname
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());  // Print the IP address
    Serial.print("ESP32 Hostname: ");
    Serial.println(WiFi.getHostname());  // Print the hostname
  } else {
    Serial.println("Failed to connect to WiFi");
  }
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  Serial.println("Scan a RFID card");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized.");
  pinMode(RELAY_PIN, OUTPUT); // Initialize the relay pin as an output

  // Add UID (ADD)
  server.on("/api/add", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("uid", true) || !request->hasParam("owner", true)) {
      request->send(400, "application/json", "{\"error\": \"Missing UID or Owner\"}");
      return;
    }
    String uid = request->getParam("uid", true)->value();
    String owner = request->getParam("owner", true)->value();

    File file = SPIFFS.open("/UIDs.csv", FILE_APPEND);
    if (file) {
      file.println(uid + "," + owner);
      file.close();
      request->send(200, "application/json", "{\"message\": \"UID added\"}");
    } else {
      request->send(500, "application/json", "{\"error\": \"Failed to open file\"}");
    }
  });

  // Delete UID (DELETE)
  server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("uid", true)) {
      request->send(400, "application/json", "{\"error\": \"Missing UID\"}");
      return;
    }
    String uidToDelete = request->getParam("uid", true)->value();
    File file = SPIFFS.open("/UIDs.csv", FILE_READ);
    if (!file) {
      request->send(500, "application/json", "{\"error\": \"Failed to open file\"}");
      return;
    }

    String newContent = "";
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (!line.startsWith(uidToDelete + ",")) {
        newContent += line + "\n";
      }
    }
    file.close();

    File outFile = SPIFFS.open("/UIDs.csv", FILE_WRITE);
    if (outFile) {
      outFile.print(newContent);
      outFile.close();
      request->send(200, "application/json", "{\"message\": \"UID deleted\"}");
    } else {
      request->send(500, "application/json", "{\"error\": \"Failed to update file\"}");
    }
  });

  // Request UID list (REQUEST)
  server.on("/api/request", HTTP_GET, [](AsyncWebServerRequest *request) {
    File file = SPIFFS.open("/UIDs.csv", FILE_READ);
    if (!file) {
      request->send(500, "application/json", "{\"error\": \"Failed to open file\"}");
      return;
    }

    String json = "[";
    bool first = true;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (!first) json += ",";
      json += "{\"uid\":\"" + line.substring(0, line.indexOf(',')) + "\",\"owner\":\"" + line.substring(line.indexOf(',') + 1) + "\"}";
      first = false;
    }
    file.close();
    json += "]";

    request->send(200, "application/json", json);
  });

  // Open door (OPEN)
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request) {
    digitalWrite(RELAY_PIN, HIGH);
    delay(1000);
    digitalWrite(RELAY_PIN, LOW);
    request->send(200, "application/json", "{\"message\": \"Door opened\"}");
  });

  server.begin();
}

void loop() {
  // Check WiFi connection status
  checkWiFiConnection();

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
        openDoor(); // Open the door if the UID is found locally
        String macAddress = WiFi.macAddress();
        sendMacAddressAndUIDToAPI(macAddress, content, true);
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