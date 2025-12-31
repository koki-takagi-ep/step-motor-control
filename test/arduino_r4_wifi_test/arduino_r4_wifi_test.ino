/*
 * Arduino R4 WiFi Communication Test
 * 
 * This sketch demonstrates WiFi communication between an Arduino R4 WiFi board and a PC:
 * 1. Creates a web server for remote control and monitoring via browser
 * 2. Establishes a TCP socket client that connects to a server running on your PC
 * 
 * NOTE: For Arduino R4 WiFi, we use the WiFiS3 library
 */

#include <WiFiS3.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";  // Replace with your WiFi password

// Web server on port 80
WiFiServer webServer(80);

// TCP client settings
const char* serverIP = "192.168.1.100";  // Replace with your PC's IP address
const int serverPort = 8888;             // Choose a port for communication
WiFiClient client;
unsigned long lastConnectionAttempt = 0;
const unsigned long connectionInterval = 5000;  // Try to connect every 5 seconds

// Pin definitions
const int ledPin = LED_BUILTIN;  // Built-in LED
const int sensorPin = A0;        // Analog input pin

// Data exchange buffer
char dataBuffer[64];
unsigned long lastDataSent = 0;
const unsigned long dataSendInterval = 1000;  // Send data every 1 second
