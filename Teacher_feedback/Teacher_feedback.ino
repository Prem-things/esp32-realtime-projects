// === LIBRARIES ===
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === LCD ===
LiquidCrystal_I2C lcd(0x27, 16, 4);

// === BUTTON PINS ===
#define BUTTON_EXCELLENT 23
#define BUTTON_GOOD      19
#define BUTTON_AVG       18
#define BUTTON_BAD       5

#define SUBJECT_ENGLISH       32
#define SUBJECT_KANNADA       33
#define SUBJECT_HINDI         25
#define SUBJECT_SCIENCE       26
#define SUBJECT_SSCIENCE      27
#define SUBJECT_MATHS         14

// === RFID ===
#define RFID_RX 4
HardwareSerial rfidSerial(1);

// === EEPROM LAYOUT ===
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 64
#define CLASS_ADDR 128
#define FLAG_ADDR 256

char wifiSSID[64], wifiPass[64], classroom[32];
WebServer server(80);
bool configMode = false;

// === SUBJECTS ===
const char* subjects[] = {"English", "Kannada", "Hindi", "Science", "SocialScience", "Maths"};
int subjectPins[] = {SUBJECT_ENGLISH, SUBJECT_KANNADA, SUBJECT_HINDI, SUBJECT_SCIENCE, SUBJECT_SSCIENCE, SUBJECT_MATHS};
int selectedSubject = -1;
bool subjectConfirmed = false;

// === FEEDBACK LABELS ===
const char* feedbackLabels[] = {"Bad", "Average", "Good", "Excellent"};
int feedbackPins[] = {BUTTON_BAD, BUTTON_AVG, BUTTON_GOOD, BUTTON_EXCELLENT};

// === CACHE ===
String usedRFIDs[100];
int usedCount = 0;

// === GOOGLE SCRIPT URL ===
const String googleScriptURL = "https://script.google.com/macros/s/AKfycbx17ln_0yqMg_0hBtIdt_ExAFNpzb7_nVWMZEIc-SB_2IQvXIZYohXqG8oE8n69LiaW/exec";

// === FUNCTION DECLARATIONS ===
void saveConfigToEEPROM();
void loadConfigFromEEPROM();
bool isConfigured();
void startConfigPortal();
void connectToWiFi();
bool isDuplicate(String rfid);
void sendFeedback(String rfid, int level);

// === EEPROM FUNCTIONS ===
void saveConfigToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 64; i++) EEPROM.write(SSID_ADDR + i, wifiSSID[i]);
  for (int i = 0; i < 64; i++) EEPROM.write(PASS_ADDR + i, wifiPass[i]);
  for (int i = 0; i < 32; i++) EEPROM.write(CLASS_ADDR + i, classroom[i]);
  EEPROM.write(FLAG_ADDR, 0xA5);
  EEPROM.commit();
  EEPROM.end();
}

void loadConfigFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 64; i++) wifiSSID[i] = EEPROM.read(SSID_ADDR + i);
  for (int i = 0; i < 64; i++) wifiPass[i] = EEPROM.read(PASS_ADDR + i);
  for (int i = 0; i < 32; i++) classroom[i] = EEPROM.read(CLASS_ADDR + i);
  EEPROM.end();
}

bool isConfigured() {
  EEPROM.begin(EEPROM_SIZE);
  bool result = EEPROM.read(FLAG_ADDR) == 0xA5;
  EEPROM.end();
  return result;
}

// === START CONFIG PORTAL ===
void startConfigPortal() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Feedback_Setup");

  server.on("/", []() {
    String html = "<form action='/save' method='POST'>"
                  "WiFi SSID:<br><input name='ssid'><br>"
                  "WiFi Password:<br><input name='pass'><br>"
                  "Classroom (e.g. 7B):<br><input name='class'><br>"
                  "<input type='submit' value='Save & Reboot'></form>";
    server.send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, []() {
    String ssidVal = server.arg("ssid");
    String passVal = server.arg("pass");
    String classVal = server.arg("class");

    ssidVal.toCharArray(wifiSSID, 64);
    passVal.toCharArray(wifiPass, 64);
    classVal.toCharArray(classroom, 32);

    saveConfigToEEPROM();
    server.send(200, "text/html", "<h3>Saved! Rebooting...</h3>");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Setup Mode On");
  lcd.setCursor(0, 1); lcd.print("Connect to AP");

  unsigned long start = millis();
  while (millis() - start < 180000) {
    server.handleClient();
  }

  lcd.clear();
  lcd.print("Timeout. Reboot");
  delay(2000);
  ESP.restart();
}

void connectToWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Connecting...");
  WiFi.begin(wifiSSID, wifiPass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
    delay(500);
    lcd.print(".");
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 0); lcd.print(String(classroom) + " Feedback");
    lcd.setCursor(0, 1); lcd.print("Select Subject");
    lcd.setCursor(0, 3); lcd.print("Waiting...");
  } else {
    lcd.print("WiFi Failed");
    delay(2000);
    startConfigPortal();
  }
}

bool isDuplicate(String rfid) {
  for (int i = 0; i < usedCount; i++) {
    if (usedRFIDs[i] == rfid) return true;
  }
  usedRFIDs[usedCount++] = rfid;
  return false;
}

void sendFeedback(String rfid, int level) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = googleScriptURL +
               "?classroom=" + String(classroom) +
               "&subject=" + String(subjects[selectedSubject]) +
               "&feedback=" + String(feedbackLabels[level]);
  http.begin(url);
  http.GET();
  http.end();
}

void checkSubjectHoldToReset() {
  for (int i = 0; i < 6; i++) {
    if (digitalRead(subjectPins[i]) == LOW) {
      delay(5000);
      if (digitalRead(subjectPins[i]) == LOW) {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Session Ended");
        delay(2000);
        ESP.restart();
      }
    }
  }
}

// === MAIN SETUP ===
void setup() {
  Serial.begin(115200);
  rfidSerial.begin(9600, SERIAL_8N1, RFID_RX, -1);
  Wire.begin(22, 21);
  lcd.init(); lcd.backlight();

  for (int i = 0; i < 6; i++) pinMode(subjectPins[i], INPUT_PULLUP);
  for (int i = 0; i < 4; i++) pinMode(feedbackPins[i], INPUT_PULLUP);

  if (!isConfigured()) startConfigPortal();
  else {
    loadConfigFromEEPROM();
    connectToWiFi();
  }
}

void loop() {
  if (configMode) return;

  checkSubjectHoldToReset();

  // Select Subject
  if (!subjectConfirmed) {
    for (int i = 0; i < 6; i++) {
      if (digitalRead(subjectPins[i]) == LOW) {
        if (selectedSubject == i) {
          subjectConfirmed = true;
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(String(subjects[i]));
          lcd.setCursor(0, 1); lcd.print("Tap your card");
        } else {
          selectedSubject = i;
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print("Subject: " + String(subjects[i]));
          lcd.setCursor(0, 1); lcd.print("Press again to confirm");
        }
        delay(500);
      }
    }
  }

  // RFID Read
  if (subjectConfirmed && rfidSerial.available()) {
    String rfid = "";
    while (rfidSerial.available()) {
      char c = rfidSerial.read();
      if (c != '\n' && c != '\r') rfid += c;
      delay(5);
    }
    rfid.trim();

    if (isDuplicate(rfid)) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Already Given!");
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(String(subjects[selectedSubject]));
      lcd.setCursor(0, 1); lcd.print("Tap your card");
    } else {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Give Feedback...");

      while (true) {
        for (int i = 0; i < 4; i++) {
          if (digitalRead(feedbackPins[i]) == LOW) {
            sendFeedback(rfid, i);
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print("Thanks!");
            delay(2000);
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print(String(subjects[selectedSubject]));
            lcd.setCursor(0, 1); lcd.print("Tap your card");
            return;
          }
        }
        delay(50);
      }
    }
  }
}
