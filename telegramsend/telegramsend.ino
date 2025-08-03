#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// WiFi credentials
const char* ssid = "alexa";
const char* password = "premprem";

// Telegram Bot info
const char* botToken = "7005533594:AAEAunfpz4rHytZi72nwy3pcnkFzxXUt0f4";   // Replace with your Bot token
const char* chatId = "1369821287";       // Replace with your chat ID

// Button
#define BUTTON_PIN D5
bool sent = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Use internal pull-up
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  Serial.println("\nWiFi connected");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && !sent) {
    Serial.println("Emergency button pressed!");
    sendTelegramMessage("🚨 EMERGENCY! Please check immediately.");
    sent = true;
    delay(1000);  // debounce
  }

  if (digitalRead(BUTTON_PIN) == HIGH) {
    sent = false;  // allow future presses
  }

  delay(100);
}

void sendTelegramMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Connection to Telegram failed.");
    return;
  }

  String url = "/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatId) + "&text=" + urlencode(message);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.telegram.org\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("Telegram message sent.");
}

// Simple URL encoder
String urlencode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      encodedString += '%';
      code1 = (c >> 4) & 0xF;
      code2 = c & 0xF;
      encodedString += char(code1 > 9 ? code1 - 10 + 'A' : code1 + '0');
      encodedString += char(code2 > 9 ? code2 - 10 + 'A' : code2 + '0');
    }
  }
  return encodedString;
}
