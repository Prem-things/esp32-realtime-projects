
/*
  RFID Attendance System with Web Setup
  Board: Arduino UNO R4 WiFi
  RFID: EM-18 on Serial1 (D0)
  Features:
    - EEPROM stores Web App URL and student details
    - Web interface to register students
    - Auto-fill on scan
    - Optional reset/delete
*/

#include <WiFiS3.h>
#include <EEPROM.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

// Replace with your WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

WiFiServer server(80);

#define MAX_STUDENTS 100
#define STUDENT_SIZE 42
#define URL_ADDR 0
#define URL_SIZE 100
#define STUDENT_START 100

String currentUID = "";
unsigned long lastScanTime = 0;
unsigned long scanDelay = 3000; // 3s debounce

struct Student {
  String uid;
  String name;
  String classSec;
  String rollNo;
};

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // EM-18 on D0

  EEPROM.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
  server.begin();
}

void loop() {
  readRFID();
  handleClient();
}

void readRFID() {
  if (Serial1.available()) {
    String uid = "";
    while (Serial1.available()) {
      char c = Serial1.read();
      if (c != '\n' && c != '\r') uid += c;
      delay(5);
    }
    if (uid.length() >= 10 && millis() - lastScanTime > scanDelay) {
      currentUID = uid;
      lastScanTime = millis();
      Serial.println("UID Detected: " + currentUID);
    }
  }
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;
  while (!client.available()) delay(1);
  String request = client.readStringUntil('\r');
  client.readStringUntil('\n'); // skip rest

  if (request.startsWith("GET / ")) {
    showMainPage(client);
  } else if (request.startsWith("POST /submit")) {
    handleSubmit(client);
  } else if (request.startsWith("GET /seturl")) {
    showURLPage(client);
  } else if (request.startsWith("POST /seturl")) {
    handleURLSave(client);
  } else if (request.startsWith("GET /delete")) {
    showDeletePage(client);
  } else if (request.startsWith("POST /delete")) {
    handleDelete(client);
  } else {
    client.println("HTTP/1.1 404 Not Found\r\n\r\n");
  }
  delay(1);
  client.stop();
}

void showMainPage(WiFiClient &client) {
  Student s = getStudent(currentUID);
  client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  client.println("<h2>Student Attendance</h2>");
  client.println("<form method='POST' action='/submit'>");
  client.println("UID: <input name='uid' value='" + currentUID + "' readonly><br>");
  client.println("Name: <input name='name' value='" + s.name + "'><br>");
  client.println("Class: <input name='class' value='" + s.classSec + "'><br>");
  client.println("Roll No: <input name='roll' value='" + s.rollNo + "'><br>");
  client.println("<input type='submit' value='Save & Send'><br>");
  client.println("</form>");
  client.println("<a href='/seturl'>Change Google Sheet URL</a><br>");
  client.println("<a href='/delete'>Delete Student</a>");
}

void showURLPage(WiFiClient &client) {
  String savedURL = readStringFromEEPROM(URL_ADDR, URL_SIZE);
  client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  client.println("<h2>Set Google Sheet URL</h2>");
  client.println("<form method='POST' action='/seturl'>");
  client.println("Script URL: <input name='url' size='80' value='" + savedURL + "'><br>");
  client.println("<input type='submit' value='Save URL'>");
  client.println("</form>");
  client.println("<a href='/'>Back</a>");
}

void showDeletePage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  client.println("<h2>Delete Student Record</h2>");
  client.println("<form method='POST' action='/delete'>");
  client.println("UID to delete: <input name='uid'><br>");
  client.println("<input type='submit' value='Delete'>");
  client.println("</form>");
  client.println("<a href='/'>Back</a>");
}

void handleSubmit(WiFiClient &client) {
  String body = readBody(client);
  String uid = getParam(body, "uid");
  String name = getParam(body, "name");
  String classSec = getParam(body, "class");
  String roll = getParam(body, "roll");

  saveStudent(uid, name, classSec, roll);

  String url = readStringFromEEPROM(URL_ADDR, URL_SIZE);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String payload = "{"uid":"" + uid + "","name":"" + name + "","classSec":"" + classSec + "","rollNo":"" + roll + "","type":"IN","time":"NA"}";
  http.POST(payload);
  http.end();

  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /\r\n\r\n");
}

void handleURLSave(WiFiClient &client) {
  String body = readBody(client);
  String url = getParam(body, "url");
  writeStringToEEPROM(URL_ADDR, url, URL_SIZE);
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /\r\n\r\n");
}

void handleDelete(WiFiClient &client) {
  String body = readBody(client);
  String uid = getParam(body, "uid");
  int index = findStudentIndex(uid);
  if (index != -1) {
    for (int i = 0; i < STUDENT_SIZE; i++) {
      EEPROM.write(STUDENT_START + index * STUDENT_SIZE + i, 0xFF);
    }
    EEPROM.commit();
  }
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /\r\n\r\n");
}

// ===== EEPROM HELPERS =====

String readStringFromEEPROM(int addr, int maxLen) {
  String s = "";
  for (int i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0xFF || c == '\0') break;
    s += c;
  }
  return s;
}

void writeStringToEEPROM(int addr, String data, int maxLen) {
  for (int i = 0; i < maxLen; i++) {
    if (i < data.length()) EEPROM.write(addr + i, data[i]);
    else EEPROM.write(addr + i, 0xFF);
  }
  EEPROM.commit();
}

void saveStudent(String uid, String name, String classSec, String rollNo) {
  int index = findStudentIndex(uid);
  if (index == -1) index = findFreeSlot();
  if (index == -1) return;

  int addr = STUDENT_START + index * STUDENT_SIZE;
  writeStringToEEPROM(addr, uid, 12);
  writeStringToEEPROM(addr + 12, name, 20);
  writeStringToEEPROM(addr + 32, classSec, 5);
  writeStringToEEPROM(addr + 37, rollNo, 5);
}

Student getStudent(String uid) {
  Student s = {"", "", "", ""};
  int index = findStudentIndex(uid);
  if (index == -1) return s;
  int addr = STUDENT_START + index * STUDENT_SIZE;
  s.uid = readStringFromEEPROM(addr, 12);
  s.name = readStringFromEEPROM(addr + 12, 20);
  s.classSec = readStringFromEEPROM(addr + 32, 5);
  s.rollNo = readStringFromEEPROM(addr + 37, 5);
  return s;
}

int findStudentIndex(String uid) {
  for (int i = 0; i < MAX_STUDENTS; i++) {
    int addr = STUDENT_START + i * STUDENT_SIZE;
    String storedUID = readStringFromEEPROM(addr, 12);
    if (storedUID == uid) return i;
  }
  return -1;
}

int findFreeSlot() {
  for (int i = 0; i < MAX_STUDENTS; i++) {
    int addr = STUDENT_START + i * STUDENT_SIZE;
    if (EEPROM.read(addr) == 0xFF) return i;
  }
  return -1;
}

// ===== Web Helpers =====

String readBody(WiFiClient &client) {
  while (!client.available()) delay(1);
  String line = "";
  while (client.available()) {
    line += char(client.read());
  }
  return line;
}

String getParam(String body, String key) {
  int i = body.indexOf(key + "=");
  if (i == -1) return "";
  int j = body.indexOf("&", i);
  String val = (j == -1) ? body.substring(i + key.length() + 1) : body.substring(i + key.length() + 1, j);
  val.replace("+", " ");
  return val;
}
