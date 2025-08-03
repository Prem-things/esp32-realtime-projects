#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>  // Updated for ESP32
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// WiFi credentials
const char* ssid = "alexa";
const char* password = "premprem";

// Telegram Bot info
const char* botToken = "7005533594:AAEAunfpz4rHytZi72nwy3pcnkFzxXUt0f4";
const char* chatId = "1369821287";

// NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);  // IST off set

// Time tracking
unsigned long lastEpoch = 0;
unsigned long lastMillis = 0;
bool isSynced = false;

// EEPROM Addresses
#define EEPROM_ADDR 0

// Emergency button
#define EMERGENCY_BUTTON 18
bool sent = false;

// Servo motor and buzzer
Servo medServo;
#define SERVO_PIN 5
#define BUZZER_PIN 4
#define CLOSE_BUTTON 19
bool doorOpen = false;
bool buzzerActive = false;

// I2C LCD (16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
  pinMode(CLOSE_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  medServo.attach(SERVO_PIN);
  medServo.write(0);

  Wire.begin(21, 22); // ESP32 SDA = GPIO21, SCL = GPIO22
  lcd.init();
  lcd.backlight();
  lcd.clear();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  timeClient.begin();
  EEPROM.begin(20);

  if (timeClient.update()) {
    lastEpoch = timeClient.getEpochTime();
    EEPROM.put(EEPROM_ADDR, lastEpoch);
    EEPROM.commit();
    lastMillis = millis();
    isSynced = true;
  } else {
    EEPROM.get(EEPROM_ADDR, lastEpoch);
    lastMillis = millis();
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
    lastEpoch = timeClient.getEpochTime();
    EEPROM.put(EEPROM_ADDR, lastEpoch);
    EEPROM.commit();
    lastMillis = millis();
    isSynced = true;
  }

  unsigned long currentMillis = millis();
  unsigned long elapsedSeconds = (currentMillis - lastMillis) / 1000;
  unsigned long currentEpoch = lastEpoch + elapsedSeconds;

  time_t localTime = currentEpoch;
  struct tm *ptm = localtime((time_t *)&localTime);

  int hour  = ptm->tm_hour;
  int min   = ptm->tm_min;
  int sec   = ptm->tm_sec;

  char timeBuffer[9];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", hour, min, sec);
  Serial.printf("Time: %s (WiFi: %s)\n", timeBuffer,
                WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time:");
  lcd.setCursor(6, 0);
  lcd.print(timeBuffer);
  lcd.setCursor(0, 1);
  lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: LOST");

  // Emergency button
  if (digitalRead(EMERGENCY_BUTTON) == LOW && !sent) {
    Serial.println("Emergency button pressed!");
    sendTelegramMessage("\xF0\x9F\x9A\xA8 EMERGENCY! Please check immediately.");
    sent = true;
    delay(1000);
  }
  if (digitalRead(EMERGENCY_BUTTON) == HIGH) {
    sent = false;
  }

  // Medicine box automatic logic
  if (!doorOpen && ((hour == 8 && min == 0) || (hour == 13 && min == 30) || (hour == 20 && min == 30))) {
    medServo.write(90);
    digitalWrite(BUZZER_PIN, HIGH);
    doorOpen = true;
    buzzerActive = true;
    Serial.println("Medicine box opened (auto)");
  }

  // Manual open/close via button
  if (digitalRead(CLOSE_BUTTON) == LOW) {
    if (doorOpen) {
      medServo.write(0);
      digitalWrite(BUZZER_PIN, LOW);
      doorOpen = false;
      buzzerActive = false;
      Serial.println("Medicine box closed manually");
    } else {
      medServo.write(90);
      digitalWrite(BUZZER_PIN, HIGH);
      doorOpen = true;
      buzzerActive = true;
      Serial.println("Medicine box opened manually");
    }
    delay(1000);
  }

  // Ensure buzzer remains on only while door is open
  if (!doorOpen && buzzerActive) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  delay(1000);
}

void sendTelegramMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();

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

String urlencode(String str) {
  String encodedString = "";
  char c;
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
