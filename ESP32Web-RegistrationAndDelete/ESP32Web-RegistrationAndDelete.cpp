#include <SPI.h>
#include <MFRC522.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#define SS_PIN  5 //SDA
#define RST_PIN 21 //RST
#define RELAY_PIN  2 // Define the relay pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

//void handleRoot();
void handleIndex();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

void setup() {
  Serial.begin(115200);  // Initialize serial communications
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  Serial.println("Scan a RFID card");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized.");

  // Configure ESP32 as an access point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  //server.on("/", handleRoot);
  server.on("/", handleIndex);
  //server.on("/index.html", handleIndex);
  server.begin();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  pinMode(RELAY_PIN, OUTPUT); // Initialize the relay pin as an output
  digitalWrite(RELAY_PIN, LOW); // Ensure the relay is off initially
}

void loop() {
  server.handleClient();
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

  // Check if the UID is known
  bool uidFound = false;
  String owner = "";
  File file = SPIFFS.open("/UIDs.csv");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      int commaIndex = line.indexOf(',');
      String uid = line.substring(0, commaIndex);
      owner = line.substring(commaIndex + 1);

      if (content.equals(uid)) {
        Serial.print("Hello ");
        Serial.println(owner);
        Serial.println("");
        uidFound = true;
        break;
      }
    }
    file.close();
  } else {
    Serial.println("Failed to open UIDs.csv");
  }

  // Send UID and owner information to the website
  if (uidFound) {
    webSocket.broadcastTXT("KNOWN:" + content + ":" + owner);
    digitalWrite(RELAY_PIN, HIGH); // Turn on the relay
    delay(5000); // Keep the relay on for 5 seconds
    digitalWrite(RELAY_PIN, LOW); // Turn off the relay
  } else {
    webSocket.broadcastTXT("UNKNOWN:" + content);
    Serial.print("UID tag (in red): ");
    Serial.println(content);
  }
}

/*void handleRoot() {
  //File file = SPIFFS.open("/login.html", "r");
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}*/

void handleIndex() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char *)payload);
    int colonIndex1 = message.indexOf(':');
    int colonIndex2 = message.indexOf(':', colonIndex1 + 1);
    String action = message.substring(0, colonIndex1);
    String uid = message.substring(colonIndex1 + 1, colonIndex2);
    String owner = message.substring(colonIndex2 + 1);

    if (action == "ADD") {
      bool uidExists = false;
      File file = SPIFFS.open("/UIDs.csv", "r");
      if (file) {
        while (file.available()) {
          String line = file.readStringUntil('\n');
          if (line.startsWith(uid + ",")) {
            uidExists = true;
            break;
          }
        }
        file.close();
      }

      if (uidExists) {
        webSocket.sendTXT(num, "UID already exists");
        Serial.println("UID already exists");
      } else {
        file = SPIFFS.open("/UIDs.csv", FILE_APPEND);
        if (file) {
          file.print(uid);
          file.print(",");
          file.println(owner);
          file.close();
          webSocket.sendTXT(num, "Successfully registered");
          Serial.println("UID and owner added to UIDs.csv");
        } else {
          Serial.println("Failed to open UIDs.csv for appending");
        }
      }
    } else if (action == "DELETE") {
      File file = SPIFFS.open("/UIDs.csv", "r");
      if (file) {
        String temp = "";
        bool uidFound = false;
        while (file.available()) {
          String line = file.readStringUntil('\n');
          if (!line.startsWith(uid + ",")) {
            temp += line + "\n";
          } else {
            uidFound = true;
          }
        }
        file.close();

        if (uidFound) {
          file = SPIFFS.open("/UIDs.csv", "w");
          if (file) {
            file.print(temp);
            file.close();
            webSocket.sendTXT(num, "User successfully deleted");
            Serial.println("UID deleted from UIDs.csv");
          } else {
            Serial.println("Failed to open UIDs.csv for writing");
          }
        } else {
          webSocket.sendTXT(num, "UID not found");
          Serial.println("UID not found");
        }
      } else {
        Serial.println("Failed to open UIDs.csv");
      }
    } else if (action == "OPEN") {
      digitalWrite(RELAY_PIN, HIGH); // Turn on the relay
      delay(5000); // Keep the relay on for 5 seconds
      digitalWrite(RELAY_PIN, LOW); // Turn off the relay
      webSocket.sendTXT(num, "OPEN");
      Serial.println("Open successfully");
    }
  }
}