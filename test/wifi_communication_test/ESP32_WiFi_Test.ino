/*
 * ESP32 WiFi Communication Test
 * This sketch establishes a WiFi connection and creates two communication options:
 * 1. A web server for remote control and monitoring via browser
 * 2. A TCP socket client that connects to a server on your PC
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";  // Replace with your WiFi password

// Web server on port 80
WebServer server(80);

// TCP client settings
const char* serverIP = "192.168.1.100";  // Replace with your PC's IP address
const int serverPort = 8888;             // Choose a port for communication
WiFiClient client;
unsigned long lastConnectionAttempt = 0;
const unsigned long connectionInterval = 5000;  // Try to connect every 5 seconds

// Pin definitions
const int ledPin = 2;     // Built-in LED on most ESP32 boards
const int sensorPin = 34; // Analog input pin (ADC)

// Data exchange buffer
char dataBuffer[64];
unsigned long lastDataSent = 0;
const unsigned long dataSendInterval = 1000;  // Send data every 1 second

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n\nESP32 WiFi Communication Test");
  
  // Initialize LED pin as output
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // LED off
  
  // Connect to WiFi network
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Define server routes
  server.on("/", handleRoot);
  server.on("/led/on", HTTP_GET, []() {
    digitalWrite(ledPin, HIGH);
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/led/off", HTTP_GET, []() {
    digitalWrite(ledPin, LOW);
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/sensor", HTTP_GET, []() {
    int value = analogRead(sensorPin);
    server.send(200, "application/json", "{\"value\":" + String(value) + "}");
  });
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("You can access the web interface at http://" + WiFi.localIP().toString());
  
  // Initial connection attempt to the TCP server
  Serial.printf("Attempting to connect to TCP server at %s:%d\n", serverIP, serverPort);
  connectToServer();
}

void loop() {
  // Handle web server clients
  server.handleClient();
  
  // Handle TCP client connection and communication
  if (WiFi.status() == WL_CONNECTED) {
    // If client is not connected, try to reconnect periodically
    if (!client.connected() && millis() - lastConnectionAttempt > connectionInterval) {
      connectToServer();
    }
    
    // If client is connected, send data periodically and check for incoming data
    if (client.connected()) {
      // Send data periodically
      if (millis() - lastDataSent > dataSendInterval) {
        int sensorValue = analogRead(sensorPin);
        snprintf(dataBuffer, sizeof(dataBuffer), "SENSOR:%d", sensorValue);
        client.println(dataBuffer);
        Serial.printf("Sent to server: %s\n", dataBuffer);
        lastDataSent = millis();
      }
      
      // Check for incoming data
      while (client.available()) {
        String command = client.readStringUntil('\n');
        command.trim();
        Serial.printf("Received from server: %s\n", command.c_str());
        
        // Process commands
        if (command == "LED:ON") {
          digitalWrite(ledPin, HIGH);
          client.println("LED:ON:OK");
        } 
        else if (command == "LED:OFF") {
          digitalWrite(ledPin, LOW);
          client.println("LED:OFF:OK");
        }
        else if (command == "GET:SENSOR") {
          int sensorValue = analogRead(sensorPin);
          client.printf("SENSOR:%d\n", sensorValue);
        }
      }
    }
  }
}

// Try to connect to the TCP server
void connectToServer() {
  Serial.printf("Connecting to server %s:%d... ", serverIP, serverPort);
  
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Connected!");
    client.println("ESP32 Client Connected!");
  } else {
    Serial.println("Connection failed!");
  }
  
  lastConnectionAttempt = millis();
}

// Root page handler for web interface
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 WiFi Test</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }";
  html += "h1 { color: #0066cc; }";
  html += ".btn { display: inline-block; padding: 10px 20px; margin: 10px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; text-decoration: none; color: white; }";
  html += ".on { background-color: #4CAF50; }";
  html += ".off { background-color: #f44336; }";
  html += ".sensor { background-color: #2196F3; }";
  html += "#sensorValue { font-size: 24px; margin: 20px; }";
  html += "</style></head><body>";
  html += "<h1>ESP32 WiFi Communication Test</h1>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<a class='btn on' href='/led/on'>LED ON</a>";
  html += "<a class='btn off' href='/led/off'>LED OFF</a>";
  html += "<a class='btn sensor' id='readSensor'>Read Sensor</a>";
  html += "<div id='sensorValue'></div>";
  html += "<script>";
  html += "document.getElementById('readSensor').addEventListener('click', function(e) {";
  html += "  e.preventDefault();";
  html += "  fetch('/sensor')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      document.getElementById('sensorValue').innerHTML = 'Sensor value: ' + data.value;";
  html += "    });";
  html += "});";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// 404 Not found handler
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}
