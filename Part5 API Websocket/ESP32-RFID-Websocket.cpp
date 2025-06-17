#include <SPI.h>
#include <MFRC522.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define SS_PIN  5 //SDA
#define RST_PIN 4 //RST
#define RELAY_PIN  2 // Define the relay pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
WebSocketsClient webSocket;

const char* ssid = "ssid"; // Set your desired SSID
const char* password = "password"; //  Set your desired password
const char* hostname = "ESP32-EN-Test";  // Set your desired hostname
const char* webSocketServerIP = "";  // Set the IP address of the WebSocket server

void openDoor();
void checkWiFiConnection();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);


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

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  String macAddress;
  String jsonPayload;
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket Connected");
      // Send MAC address to WebSocket server
      macAddress = WiFi.macAddress();
      jsonPayload = "{\"mac_address\":\"" + macAddress + "\"}";
      webSocket.sendTXT(jsonPayload);
      break;
    case WStype_TEXT:
      Serial.printf("WebSocket Message: %s\n", payload);
      // Parse the response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        const char* status = doc["status"];
        if (strcmp(status, "open") == 0) {
          openDoor(); // Open the door if the status is "open"
        } else if (strcmp(status, "success") == 0) {
          openDoor(); // Open the door if the attendance log is successful
          const char* uid = doc["uid"];
          const char* owner = doc["owner"];
          if (uid && owner) {
            // Save the new UID and owner to UIDs.csv
            File file = SPIFFS.open("/UIDs.csv", FILE_APPEND);
            if (file) {
              file.printf("%s,%s\n", uid, owner);
              file.close();
              Serial.println("New UID and owner saved to UIDs.csv");
            } else {
              Serial.println("Failed to open UIDs.csv for writing");
            }
          } else {
            const char* message = doc["message"];
            if (message) {
              Serial.printf("Error: %s\n", message);
            }
          }
        }
      }
      break;
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
  //digitalWrite(RELAY_PIN, HIGH); // Turn off the relay

  webSocket.begin(webSocketServerIP, 5000, "/ws");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  // Check WiFi connection status
  checkWiFiConnection();

  webSocket.loop();

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

  // Add a delay to allow time for communication with the API
  delay(1000); // Delay for 500 milliseconds

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
        // Send UID, MAC address, and status to WebSocket server for logging
        String macAddress = WiFi.macAddress();
        String jsonPayload = "{\"uid\":\"" + content + "\",\"mac_address\":\"" + macAddress + "\",\"status\":true}";
        webSocket.sendTXT(jsonPayload);
        break;
      }
    }
    file.close();
  } else {
    Serial.println("Failed to open UIDs.csv");
  }

  if (!uidFound) {
    // Send UID, MAC address, and status to WebSocket server for checking in the database
    String macAddress = WiFi.macAddress();
    String jsonPayload = "{\"uid\":\"" + content + "\",\"mac_address\":\"" + macAddress + "\",\"status\":false}";
    webSocket.sendTXT(jsonPayload);
  }
}