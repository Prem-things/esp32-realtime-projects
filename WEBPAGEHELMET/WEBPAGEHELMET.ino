#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <TinyGPS++.h>

// Replace with your desired Access Point credentials
const char* ssid = "ESP32_AP";        // Access Point Name
const char* password = "esp32password"; // Access Point Password

// Create a web server on port 80
WebServer server(80);

// GPS setup
TinyGPSPlus gps;
HardwareSerial mySerial(1);

// Sensor values
int sensorValueV2 = 0;
int sensorValueMQ6 = 0;
float temperature = 0.0;
float latitude = 0.0;
float longitude = 0.0;

void setup() {
  // Start serial communication
  Serial.begin(115200);
  // Initialize I2C for other sensors (if needed)
  Wire.begin(21, 22);

  // Initialize GPS serial communication
  mySerial.begin(9600, SERIAL_8N1, 16, 17);

  // Set up the Wi-Fi access point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Define a handler for the root page ("/")
  server.on("/", HTTP_GET, handleRoot);
  server.on("/getdata", HTTP_GET, handleGetData);

  // Start the server
  server.begin();
}

void loop() {
  // Handle any incoming client requests
  server.handleClient();

  // Read GPS data
  while (mySerial.available() > 0) {
    gps.encode(mySerial.read());
  }

  // If new GPS data is available, update the values
  if (gps.location.isUpdated()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  // Read sensor data (gas sensors, temperature)
  sensorValueV2 = analogRead(34);  // Example pin for DFRobot Gas Sensor V2
  sensorValueMQ6 = analogRead(35); // Example pin for MQ-6 Gas Sensor
  temperature = readTemperature();
}

void handleRoot() {
  String html = "<html><head><title>ESP32 Sensor Readings</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 0; color: #333; }";
  html += "h1 { text-align: center; color: #2C3E50; margin-top: 50px; }";
  html += "div { text-align: center; font-size: 20px; margin: 10px 0; }";
  html += "span { font-weight: bold; font-size: 22px; color: #1ABC9C; }";
  html += "#container { display: flex; flex-direction: column; align-items: center; padding: 20px; }";
  html += "#data { background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1); margin-top: 30px; }";
  html += "#data div { margin-bottom: 15px; }";
  html += "#data span { color: #2980B9; }";
  html += ".footer { text-align: center; margin-top: 40px; color: #95A5A6; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div id='container'>";
  html += "<h1>ESP32 Sensor Readings</h1>";
  html += "<div id='data'>";
  html += "<div>Latitude: <span id='lat'>" + String(latitude, 6) + "</span></div>";
  html += "<div>Longitude: <span id='lng'>" + String(longitude, 6) + "</span></div>";
  html += "<div>DFRobot Gas Sensor V2 Value: <span id='sensorV2'>" + String(sensorValueV2) + "</span></div>";
  html += "<div>MQ-6 Gas Sensor Value: <span id='sensorMQ6'>" + String(sensorValueMQ6) + "</span></div>";
  html += "<div>Temperature from AS6212: <span id='temperature'>" + String(temperature) + "</span></div>";
  html += "</div>";
  html += "</div>";
  html += "<div class='footer'>Powered by ESP32</div>";
  html += "<script>";
  html += "setInterval(function(){fetch('/getdata').then(response => response.json()).then(data => {";
  html += "document.getElementById('lat').innerText = data.latitude;";
  html += "document.getElementById('lng').innerText = data.longitude;";
  html += "document.getElementById('sensorV2').innerText = data.sensorV2;";
  html += "document.getElementById('sensorMQ6').innerText = data.sensorMQ6;";
  html += "document.getElementById('temperature').innerText = data.temperature;});}, 1000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Handle the AJAX request to fetch sensor data
void handleGetData() {
  String jsonData = "{\"latitude\":" + String(latitude, 6) + ",";
  jsonData += "\"longitude\":" + String(longitude, 6) + ",";
  jsonData += "\"sensorV2\":" + String(sensorValueV2) + ",";
  jsonData += "\"sensorMQ6\":" + String(sensorValueMQ6) + ",";
  jsonData += "\"temperature\":" + String(temperature) + "}";

  server.send(200, "application/json", jsonData);
}

// Function to read the temperature from the AS6212
float readTemperature() {
  Wire.beginTransmission(0x48);  // AS6212 I2C address
  Wire.write(0x00);  // Start reading from the temperature register
  Wire.endTransmission();
  
  Wire.requestFrom(0x48, 2);
  if (Wire.available() == 2) {
    uint16_t tempData = Wire.read() << 8 | Wire.read();
    return (tempData * 0.0078125);  // AS6212 temperature conversion factor
  }
  return -1;  // Return error value if read fails
}
