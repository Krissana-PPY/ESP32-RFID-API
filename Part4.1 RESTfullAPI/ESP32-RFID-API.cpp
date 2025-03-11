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

const char* ssid = "WIFI"; // Set your desired SSID
const char* password = "password"; // Set your desired password
const char* hostname = "ESP32-EN4111";  // Set your desired hostname
const char* apiIPAddress = "http://ip address:5000"; // Set the IP address of the API server

AsyncWebServer server(80);

void checkWiFiConnection();
void updateIPAddress(const String& macAddress, const String& ipAddress);
void sendDATA(const String& macAddress, const String& uid, bool uidFound);
void openDoor();

void sendDATA(const String& macAddress, const String& uid, bool uidFound) {
  HTTPClient http;
  int httpResponseCode = -1;
  int retryCount = 0;
  while (httpResponseCode <= 0 && retryCount < 5) { // Retry up to 5 times
    http.begin(String(apiIPAddress) + "/api/send_uid"); // Use the apiIPAddress variable
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"mac_address\": \"" + macAddress + "\", \"uid\": \"" + uid + "\", \"uid_found\": " + (uidFound ? "true" : "false") + "}";
    httpResponseCode = http.POST(payload);

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
      delay(1000); // Wait for 1 second before retrying
      retryCount++;
    }

    http.end();
  }
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

void updateIPAddress(const String& macAddress, const String& ipAddress) {
  HTTPClient http;
  int httpResponseCode = -1;
  int retryCount = 0;
  while (httpResponseCode <= 0 && retryCount < 5) { // Retry up to 5 times
    http.begin(String(apiIPAddress) + "/api/update_ip"); // Use the apiIPAddress variable
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"mac_address\": \"" + macAddress + "\", \"ip_address\": \"" + ipAddress + "\"}";
    httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Server response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
      delay(1000); // Wait for 1 second before retrying
      retryCount++;
    }

    http.end();
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
  // curl -X POST http://ip_address_ESP32:80/api/add --data-urlencode "uid=73A0301D" --data-urlencode "owner=NAME"
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
      Serial.println(uid + " and " + owner + " added to UIDs.csv");
    } else {
      request->send(500, "application/json", "{\"error\": \"Failed to open file\"}");
    }
  });

  // Delete UID (DELETE)
  // curl -X POST http://ip_address_ESP32:80/api/delete --data-urlencode "uid=73A0301D"
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
      Serial.println(uidToDelete + " deleted from UIDs.csv");
    } else {
      request->send(500, "application/json", "{\"error\": \"Failed to update file\"}");
    }
  });

  // Request UID list (REQUEST)
  // curl -X GET http://ip_address_ESP32:80/api/request
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
    Serial.println("UID list sent");
  });

  // Open door (OPEN)
  // curl -X POST http://ip_address_ESP32:80/api/open
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request) {
    openDoor();
    request->send(200, "application/json", "{\"message\": \"Door opened\"}");
    Serial.println("Door opened");
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
        sendDATA(macAddress, content, true);
        break;
      }
    }
    file.close();
  } else {
    Serial.println("Failed to open UIDs.csv");
  }

  if (!uidFound) {
    String macAddress = WiFi.macAddress();
    sendDATA(macAddress, content, false);
  }
}