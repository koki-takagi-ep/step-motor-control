/*
 * ESP8266 WiFi Communication Test
 * This sketch establishes a WiFi connection and sets up a simple web server
 * that allows LED control and sensor reading via a web interface.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";  // Replace with your WiFi password

// Web server on port 80
ESP8266WebServer server(80);

// Pin definitions
const int ledPin = 2;  // Built-in LED on most ESP8266 boards (LED_BUILTIN)
const int sensorPin = A0;  // Analog input pin

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n\nESP8266 WiFi Communication Test");
  
  // Initialize LED pin as output
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // LED off (ESP8266 built-in LED is active LOW)
  
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
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.on("/sensor", handleSensor);
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle client requests
  server.handleClient();
}

// Root page handler
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP8266 WiFi Test</title>";
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
  html += "<h1>ESP8266 WiFi Communication Test</h1>";
  html += "<a class='btn on' href='/led/on'>LED ON</a>";
  html += "<a class='btn off' href='/led/off'>LED OFF</a>";
  html += "<a class='btn sensor' href='/sensor'>Read Sensor</a>";
  html += "<div id='sensorValue'></div>";
  html += "<script>";
  html += "document.querySelector('.sensor').addEventListener('click', function(e) {";
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

// LED ON handler
void handleLedOn() {
  digitalWrite(ledPin, LOW);  // Turn LED on (active LOW)
  server.sendHeader("Location", "/");
  server.send(303);
}

// LED OFF handler
void handleLedOff() {
  digitalWrite(ledPin, HIGH);  // Turn LED off
  server.sendHeader("Location", "/");
  server.send(303);
}

// Sensor read handler
void handleSensor() {
  int sensorValue = analogRead(sensorPin);
  String json = "{\"value\":" + String(sensorValue) + "}";
  server.send(200, "application/json", json);
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
