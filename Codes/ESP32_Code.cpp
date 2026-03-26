#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SS_PIN    5
#define RST_PIN   4
#define GREEN_LED 16
#define RED_LED   17
#define BUZZER    15

// OLED 128x32
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- FILL THESE IN ----
const char* ssid      = "WIFI Username";
const char* password  = "WIFI Password";
const char* scriptURL = "Here is your App script URL";
// -----------------------

MFRC522 rfid(SS_PIN, RST_PIN);

struct Student {
  byte uid[4];
  String name;
  String rollNo;
  String branch; // "CSE", "ECE", "IE"
};

Student students[] = {
  {{0x29, 0xD6, 0xF2, 0xB7}, "Name1",     "Roll No1", "Sheet1"},
  {{0x39, 0xBF, 0xE1, 0xB7}, "Name2", "Roll No2", "Sheet2"},
  {{0x29, 0xC0, 0xBE, 0xB7}, "Name3",       "Roll No3", "Sheet3"},
  {{0x44, 0x90, 0xCA, 0x06}, "Sheet4",       "Roll No4",    "Sheet4"}, // ← no branch
};

int studentCount = sizeof(students) / sizeof(students[0]);
int siNo = 1;

void setup() {
  Serial.begin(115200);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  Wire.begin(21, 22);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // WiFi connect
  showOLED("Connecting...", "WiFi", "", "");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Time sync
  configTime(19800, 0, "pool.ntp.org");
  struct tm t;
  while (!getLocalTime(&t)) {
    delay(500); Serial.println("Syncing time...");
  }
  Serial.println("Time synced!");

  showOLED("RFID Attendance", "System Ready", "", "Scan Card...");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  int idx = findStudent();

  if (idx >= 0) {

  //  STEP 1: Show "Updating..." immediately
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  showOLED(
    "Updating...",
    students[idx].name.substring(0, 16),
    students[idx].branch,
    ""
  );

  // Optional small beep
  tone(BUZZER, 1500, 100);

  // Small delay for visual clarity
  for (int i = 0; i < 3; i++) {
  showOLED("Updating.", students[idx].name, "", "");
  delay(200);
  showOLED("Updating..", students[idx].name, "", "");
  delay(200);
  showOLED("Updating...", students[idx].name, "", "");
  delay(200);
}

  // STEP 2: Now proceed with sending data
  String dateStr = getDate();
  String timeStr = getTime();

  String response = sendToSheets(
    siNo,
    students[idx].name,
    students[idx].rollNo,
    students[idx].branch,
    dateStr,
    timeStr
  );

    if (response == "ENTRY") {
      siNo++;
      entrySuccess(students[idx].name, students[idx].branch);
    } else if (response == "EXIT") {
      exitSuccess(students[idx].name, students[idx].branch);
    } else {
      networkError();
    }

  } else {
    invalidCard();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(2000);

  // Back to idle
  showOLED("RFID Attendance", "", "", "Scan Card...");
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
}

// ---- Find student by UID ----
int findStudent() {
  for (int i = 0; i < studentCount; i++) {
    bool match = true;
    for (byte j = 0; j < 4; j++) {
      if (rfid.uid.uidByte[j] != students[i].uid[j]) {
        match = false; break;
      }
    }
    if (match) return i;
  }
  return -1;
}

// ---- Send to Google Sheets ----
String sendToSheets(int si, String name, String roll, String branch, String date, String time) {
  if (WiFi.status() != WL_CONNECTED) return "ERROR";

  HTTPClient http;
  String url = String(scriptURL) +
    "?si="     + si     +
    "&name="   + name   +
    "&roll="   + roll   +
    "&branch=" + branch +
    "&date="   + date   +
    "&time="   + time;
  url.replace(" ", "%20");

  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = http.GET();
  String response = "ERROR";

  if (code > 0) {
    response = http.getString();
    response.trim();
    Serial.println("HTTP: " + String(code) + " | " + response);
  } else {
    Serial.println("Request failed: " + String(code));
  }

  http.end();
  return response;
}

// ---- OLED Display Helper ----
void showOLED(String line1, String line2, String line3, String line4) {
  display.clearDisplay();
  display.setTextSize(1);

  if (line1 != "") { display.setCursor(0, 0);  display.println(line1); }
  if (line2 != "") { display.setCursor(0, 8);  display.println(line2); }
  if (line3 != "") { display.setCursor(0, 16); display.println(line3); }
  if (line4 != "") { display.setCursor(0, 24); display.println(line4); }

  display.display();
}

// ---- Valid Entry ----
void entrySuccess(String name, String branch) {
  showOLED("Access Granted", name.substring(0, 16), branch, "Entry Recorded!");
  digitalWrite(GREEN_LED, HIGH);  // solid green
  digitalWrite(RED_LED, LOW);
  // Pleasant double beep
  tone(BUZZER, 1800, 150); delay(200);
  tone(BUZZER, 2200, 150); delay(200);
  noTone(BUZZER);
}

// ---- Valid Exit ----
void exitSuccess(String name, String branch) {
  showOLED("Visit Again!", name.substring(0, 16), branch, "Exit Recorded!");
  digitalWrite(GREEN_LED, HIGH);  // solid green
  digitalWrite(RED_LED, LOW);
  // Single pleasant beep
  tone(BUZZER, 2000, 300); delay(350);
  noTone(BUZZER);
}

// ---- Invalid Card ----
void invalidCard() {
  showOLED("Invalid Card!", "Access Denied", "", "");
  digitalWrite(GREEN_LED, LOW);
  // Blink red LED 3 times + harsh buzzer
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 400, 200);
    delay(250);
    digitalWrite(RED_LED, LOW);
    noTone(BUZZER);
    delay(150);
  }
}

// ---- Network Error ----
void networkError() {
  showOLED("Updated!", "Thank You...", "", "");
  digitalWrite(RED_LED, HIGH);
  tone(BUZZER, 600, 500);
  delay(600);
  noTone(BUZZER);
  digitalWrite(RED_LED, LOW);
}

// ---- Date & Time ----
String getDate() {
  struct tm t;
  getLocalTime(&t);
  char buf[12];
  strftime(buf, sizeof(buf), "%d/%m/%Y", &t);
  return String(buf);
}

String getTime() {
  struct tm t;
  getLocalTime(&t);
  char buf[10];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  return String(buf);
}
