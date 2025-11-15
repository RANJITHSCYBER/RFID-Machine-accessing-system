// ESP32 RFID Access System with Relay, Push Button, Web Server (SoftAP), and Manual DS3231 RTC
// Features:
// - RFID (MFRC522) to identify student (roll + name)
// - Relay control to power machine ON/OFF
// - Push button to end session; logs usage to EEPROM with timestamps from DS3231
// - Web server at 192.168.4.1: Modern Dashboard, Users Management, Logs, Statistics
// - DS3231 accessed directly via Wire (no RTClib/BusIO dependency)
// - **All data stored in ESP32 EEPROM (Preferences) - No SD card required**
// - Advanced RFID card management via web interface
//
// Required libraries (Arduino IDE Library Manager):
// - MFRC522 by Miguel Balboa
// - SPI (from ESP32 core)
// - WiFi (from ESP32 core)
// - WebServer (from ESP32 core)
// - Wire (from ESP32 core)
//
// Adjust pins to your wiring.

#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>

// ------------------------ Pin Configuration ------------------------
#define PIN_RFID_SS     21     // MFRC522 SS (SDA)
#define PIN_RFID_RST    22     // MFRC522 RST
#define PIN_RELAY       26     // Relay output (active HIGH)
#define PIN_BUTTON      27     // End-session push button (to GND), internal pull-up
#define PIN_BUZZER      25     // Passive buzzer (PWM capable pin)

// I2C for DS3231 (use alt pins to avoid 21/22 conflict)
#define I2C_SDA         32
#define I2C_SCL         33
#define DS3231_ADDRESS  0x68

// ------------------------ WiFi SoftAP ------------------------
const char* AP_SSID = "MachineAccess";
const char* AP_PASS = "12345678";
WebServer server(80);
DNSServer dnsServer;

// ------------------------ RFID ------------------------
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
// I2C LCD (common address 0x27). Change if your module uses 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------------ Data Structures ------------------------
struct User {
  String uid;   // UID as hex string
  String roll;
  String name;
  unsigned long createdAt; // timestamp when user was created
};

struct RTCDateTime {
  uint16_t year; // 2000-2099
  uint8_t month; // 1-12
  uint8_t day;   // 1-31
  uint8_t hour;  // 0-23
  uint8_t minute;// 0-59
  uint8_t second;// 0-59
};

struct Session {
  bool active;
  String uid;
  String roll;
  String name;
  RTCDateTime startTime;
  unsigned long startMillis;
};

struct LogEntry {
  String startStr;
  String endStr;
  unsigned long durationSec;
  String uid;
  String roll;
  String name;
};

static const size_t MAX_USERS = 500; // EEPROM limit consideration
static const size_t MAX_LOGS = 1000;  // EEPROM limit consideration
std::vector<User> users;
std::vector<LogEntry> logs;
Session currentSession = { false, "", "", "", {0,0,0,0,0,0}, 0 };

// Track last scanned UID for web form to poll
String lastScannedUID = "";
unsigned long lastScannedTime = 0;

// Relay state tracking for diagnostics
bool relayState = false; // false=OFF, true=ON
// Relay configuration: 
// Most relay modules with transistor protection (BC547) are ACTIVE LOW
// LOW signal = Relay ON, HIGH signal = Relay OFF
// If your relay module has a transistor (BC547), set this to FALSE
// If relay doesn't work, try toggling this value
const bool RELAY_ACTIVE_HIGH = false; // Set to true if relay is active HIGH (uncommon)

// Preferences (EEPROM) for all data storage
Preferences preferences;
const char* PREF_NAMESPACE = "rfid_sys";
const char* PREF_USER_COUNT = "user_count";
const char* PREF_LOG_COUNT = "log_count";

// ------------------------ Helpers ------------------------
String pad2(int v) { return (v < 10) ? ("0" + String(v)) : String(v); }

String formatDateTime(const RTCDateTime& dt) {
  // "YYYY-MM-DD HH:MM:SS"
  return String(dt.year) + "-" + pad2(dt.month) + "-" + pad2(dt.day) + " " +
         pad2(dt.hour) + ":" + pad2(dt.minute) + ":" + pad2(dt.second);
}

uint8_t decToBcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
uint8_t bcdToDec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

// Read DS3231 time into RTCDateTime
bool rtcRead(RTCDateTime& out) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(0x00); // seconds register
  if (Wire.endTransmission() != 0) return false;

  // Request 7 bytes: sec, min, hr, dow, day, month, year
  if (Wire.requestFrom(DS3231_ADDRESS, (uint8_t)7) != 7) return false;
  uint8_t sec   = Wire.read() & 0x7F;
  uint8_t min   = Wire.read() & 0x7F;
  uint8_t hrReg = Wire.read();
  Wire.read(); // dow (ignored)
  uint8_t day   = Wire.read() & 0x3F;
  uint8_t month = Wire.read() & 0x1F;
  uint8_t year  = Wire.read(); // 0-99

  // 24-hour mode handling
  uint8_t hour;
  if (hrReg & 0x40) { // 12-hour mode
    hour = bcdToDec(hrReg & 0x1F);
    bool pm = hrReg & 0x20;
    if (pm && hour < 12) hour += 12;
    if (!pm && hour == 12) hour = 0;
  } else { // 24-hour mode
    hour = bcdToDec(hrReg & 0x3F);
  }

  out.second = bcdToDec(sec);
  out.minute = bcdToDec(min);
  out.hour   = hour;
  out.day    = bcdToDec(day);
  out.month  = bcdToDec(month);
  out.year   = 2000 + bcdToDec(year);
  return true;
}

// Set DS3231 time (useful via web form)
bool rtcWrite(const RTCDateTime& in) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(0x00); // start at seconds
  Wire.write(decToBcd(in.second));
  Wire.write(decToBcd(in.minute));
  Wire.write(decToBcd(in.hour)); // 24-hour mode
  Wire.write(decToBcd(1));       // day-of-week placeholder
  Wire.write(decToBcd(in.day));
  Wire.write(decToBcd(in.month));
  Wire.write(decToBcd((uint8_t)(in.year - 2000)));
  return Wire.endTransmission() == 0;
}

String uidHexFromRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return "";
  char buff[3];
  String s = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    sprintf(buff, "%02X", rfid.uid.uidByte[i]);
    s += buff;
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return s; // hex UID (e.g., "A1B2C3D4")
}

int findUserIndexByUID(const String& uid) {
  for (size_t i = 0; i < users.size(); i++) {
    if (users[i].uid == uid) return (int)i;
  }
  return -1;
}

// ======================== EEPROM (Preferences) Functions ========================
void saveUserToEEPROM(const User& u, int index) {
  preferences.begin(PREF_NAMESPACE, false); // read-write
  String uidKey = "user_uid_" + String(index);
  String rollKey = "user_roll_" + String(index);
  String nameKey = "user_name_" + String(index);
  String timeKey = "user_time_" + String(index);
  preferences.putString(uidKey.c_str(), u.uid);
  preferences.putString(rollKey.c_str(), u.roll);
  preferences.putString(nameKey.c_str(), u.name);
  preferences.putULong64(timeKey.c_str(), u.createdAt);
  preferences.end();
}

void loadUsersFromEEPROM() {
  users.clear();
  preferences.begin(PREF_NAMESPACE, true); // read-only
  int count = preferences.getInt(PREF_USER_COUNT, 0);
  for (int i = 0; i < count && i < MAX_USERS; i++) {
    String uidKey = "user_uid_" + String(i);
    String rollKey = "user_roll_" + String(i);
    String nameKey = "user_name_" + String(i);
    String timeKey = "user_time_" + String(i);
    String uid = preferences.getString(uidKey.c_str(), "");
    String roll = preferences.getString(rollKey.c_str(), "");
    String name = preferences.getString(nameKey.c_str(), "");
    unsigned long createdAt = preferences.getULong64(timeKey.c_str(), 0);
    if (uid.length() > 0) {
      users.push_back({uid, roll, name, createdAt});
    }
  }
  preferences.end();
  Serial.print("Loaded "); Serial.print(users.size()); Serial.println(" users from EEPROM");
}

void saveAllUsersToEEPROM() {
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt(PREF_USER_COUNT, users.size());
  for (size_t i = 0; i < users.size(); i++) {
    saveUserToEEPROM(users[i], i);
  }
  preferences.end();
  Serial.println("All users saved to EEPROM");
}

void saveLogToEEPROM(const LogEntry& log, int index) {
  preferences.begin(PREF_NAMESPACE, false);
  String startKey = "log_start_" + String(index);
  String endKey = "log_end_" + String(index);
  String durKey = "log_dur_" + String(index);
  String uidKey = "log_uid_" + String(index);
  String rollKey = "log_roll_" + String(index);
  String nameKey = "log_name_" + String(index);
  preferences.putString(startKey.c_str(), log.startStr);
  preferences.putString(endKey.c_str(), log.endStr);
  preferences.putULong64(durKey.c_str(), log.durationSec);
  preferences.putString(uidKey.c_str(), log.uid);
  preferences.putString(rollKey.c_str(), log.roll);
  preferences.putString(nameKey.c_str(), log.name);
  preferences.end();
}

void loadLogsFromEEPROM() {
  logs.clear();
  preferences.begin(PREF_NAMESPACE, true);
  int count = preferences.getInt(PREF_LOG_COUNT, 0);
  for (int i = 0; i < count && i < MAX_LOGS; i++) {
    String startKey = "log_start_" + String(i);
    String endKey = "log_end_" + String(i);
    String durKey = "log_dur_" + String(i);
    String uidKey = "log_uid_" + String(i);
    String rollKey = "log_roll_" + String(i);
    String nameKey = "log_name_" + String(i);
    LogEntry entry;
    entry.startStr = preferences.getString(startKey.c_str(), "");
    entry.endStr = preferences.getString(endKey.c_str(), "");
    entry.durationSec = preferences.getULong64(durKey.c_str(), 0);
    entry.uid = preferences.getString(uidKey.c_str(), "");
    entry.roll = preferences.getString(rollKey.c_str(), "");
    entry.name = preferences.getString(nameKey.c_str(), "");
    if (entry.startStr.length() > 0) {
      logs.push_back(entry);
    }
  }
  preferences.end();
  Serial.print("Loaded "); Serial.print(logs.size()); Serial.println(" logs from EEPROM");
}

void appendLogToEEPROM(const LogEntry& log) {
  if (logs.size() >= MAX_LOGS) {
    // Remove oldest log (FIFO)
    logs.erase(logs.begin());
    // Shift all logs in EEPROM
    preferences.begin(PREF_NAMESPACE, false);
    for (size_t i = 0; i < logs.size(); i++) {
      saveLogToEEPROM(logs[i], i);
    }
    preferences.end();
  }
  
  logs.push_back(log);
  int index = logs.size() - 1;
  saveLogToEEPROM(log, index);
  
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt(PREF_LOG_COUNT, logs.size());
  preferences.end();
  
  Serial.println("Log appended to EEPROM: " + log.startStr);
}

// ------------------------ Machine Control ------------------------
void machineOn() {
  Serial.println("=== ACTIVATING RELAY ===");
  Serial.println("Pin: " + String(PIN_RELAY));
  Serial.println("Configuration: " + String(RELAY_ACTIVE_HIGH ? "ACTIVE HIGH" : "ACTIVE LOW"));
  
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(PIN_RELAY, HIGH);
    Serial.println("Setting pin to HIGH (ON)");
  } else {
    digitalWrite(PIN_RELAY, LOW);
    Serial.println("Setting pin to LOW (ON - active LOW module)");
  }
  
  delay(10); // Small delay to ensure signal is set
  relayState = true;
  
  int pinRead = digitalRead(PIN_RELAY);
  Serial.println("Pin state after write: " + String(pinRead));
  Serial.println("Expected: " + String(RELAY_ACTIVE_HIGH ? "1 (HIGH)" : "0 (LOW)"));
  Serial.println("Relay should be: ON");
  Serial.println("=========================");
}

void machineOff() {
  Serial.println("=== DEACTIVATING RELAY ===");
  Serial.println("Pin: " + String(PIN_RELAY));
  
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(PIN_RELAY, LOW);
    Serial.println("Setting pin to LOW (OFF)");
  } else {
    digitalWrite(PIN_RELAY, HIGH);
    Serial.println("Setting pin to HIGH (OFF - active LOW module)");
  }
  
  delay(10); // Small delay to ensure signal is set
  relayState = false;
  
  int pinRead = digitalRead(PIN_RELAY);
  Serial.println("Pin state after write: " + String(pinRead));
  Serial.println("Expected: " + String(RELAY_ACTIVE_HIGH ? "0 (LOW)" : "1 (HIGH)"));
  Serial.println("Relay should be: OFF");
  Serial.println("===========================");
}

// Test relay by cycling it ON and OFF
void testRelay() {
  Serial.println("\n*** RELAY TEST SEQUENCE STARTING ***");
  Serial.println("Testing relay on pin " + String(PIN_RELAY));
  
  // Test 1: Turn ON
  Serial.println("\n[TEST 1] Turning relay ON...");
  machineOn();
  delay(1000); // Keep ON for 1 second
  
  // Test 2: Turn OFF
  Serial.println("\n[TEST 2] Turning relay OFF...");
  machineOff();
  delay(1000); // Keep OFF for 1 second
  
  // Test 3: Turn ON again
  Serial.println("\n[TEST 3] Turning relay ON again...");
  machineOn();
  delay(1000);
  
  // Test 4: Turn OFF
  Serial.println("\n[TEST 4] Turning relay OFF...");
  machineOff();
  
  Serial.println("\n*** RELAY TEST SEQUENCE COMPLETE ***");
  Serial.println("If you heard relay clicks, it's working!");
  Serial.println("If no clicks, check wiring and configuration.\n");
}

// Format duration in seconds to HH:MM:SS or MM:SS format
String formatDuration(unsigned long seconds) {
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  
  String result = "";
  if (hours > 0) {
    result += String(hours) + "h ";
  }
  if (minutes > 0 || hours > 0) {
    result += String(minutes) + "m ";
  }
  result += String(secs) + "s";
  return result;
}

// Format duration for LCD (compact format, max 16 chars)
String formatDurationLCD(unsigned long seconds) {
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  
  String result = "";
  if (hours > 0) {
    result = String(hours) + "h" + pad2(minutes) + "m" + pad2(secs) + "s";
  } else if (minutes > 0) {
    result = String(minutes) + "m" + pad2(secs) + "s";
  } else {
    result = String(secs) + "s";
  }
  
  // Ensure it fits in 16 characters
  if (result.length() > 16) {
    result = result.substring(0, 16);
  }
  return result;
}

// ------------------------ Buzzer Control ------------------------
// Buzzer frequency constants
#define BUZZER_FREQ_LOW    500   // Low tone frequency (Hz)
#define BUZZER_FREQ_MID    1000  // Medium tone frequency (Hz)
#define BUZZER_FREQ_HIGH   2000  // High tone frequency (Hz)
#define BUZZER_FREQ_SUCCESS 1500 // Success tone frequency (Hz)
#define BUZZER_FREQ_ERROR   800  // Error tone frequency (Hz)

void buzzerBeep(int frequency, int duration) {
  tone(PIN_BUZZER, frequency, duration);
  delay(duration);
  noTone(PIN_BUZZER);
}

void buzzerBeepPattern(int frequency, int duration, int count, int pause) {
  for (int i = 0; i < count; i++) {
    tone(PIN_BUZZER, frequency, duration);
    delay(duration + pause);
    noTone(PIN_BUZZER);
  }
}

// System ready beep (startup)
void buzzerSystemReady() {
  // Two-tone ascending beep
  tone(PIN_BUZZER, BUZZER_FREQ_MID, 200);
  delay(200);
  noTone(PIN_BUZZER);
  delay(50);
  tone(PIN_BUZZER, BUZZER_FREQ_HIGH, 200);
  delay(200);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer: System ready");
}

// RFID card detected - success (registered user)
void buzzerCardSuccess() {
  // Short high-pitched beep
  tone(PIN_BUZZER, BUZZER_FREQ_SUCCESS, 150);
  delay(150);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer: Card success");
}

// RFID card detected - unknown/error
void buzzerCardError() {
  // Low error beep (two short beeps)
  buzzerBeepPattern(BUZZER_FREQ_ERROR, 100, 2, 50);
  Serial.println("Buzzer: Card error/unknown");
}

// Session started (machine ON)
void buzzerSessionStart() {
  // Ascending two-tone beep
  tone(PIN_BUZZER, BUZZER_FREQ_MID, 150);
  delay(150);
  noTone(PIN_BUZZER);
  delay(30);
  tone(PIN_BUZZER, BUZZER_FREQ_HIGH, 150);
  delay(150);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer: Session started");
}

// Session ended (machine OFF)
void buzzerSessionEnd() {
  // Descending two-tone beep
  tone(PIN_BUZZER, BUZZER_FREQ_HIGH, 150);
  delay(150);
  noTone(PIN_BUZZER);
  delay(30);
  tone(PIN_BUZZER, BUZZER_FREQ_MID, 150);
  delay(150);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer: Session ended");
}

// Button pressed
void buzzerButtonPress() {
  // Short click sound
  tone(PIN_BUZZER, BUZZER_FREQ_MID, 50);
  delay(50);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer: Button pressed");
}

// ------------------------ Modern Web UI ------------------------
String htmlHeader(String title) {
  return String("<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'><meta name='mobile-web-app-capable' content='yes'><meta name='apple-mobile-web-app-capable' content='yes'>")
    + "<title>" + title + " - RFID Access Control</title>"
    + "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;}"
    "body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:10px;}"
    ".container{max-width:1200px;margin:0 auto;background:#fff;border-radius:15px;box-shadow:0 20px 60px rgba(0,0,0,0.3);overflow:hidden;}"
    ".header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:20px;text-align:center;}"
    ".header h1{font-size:2em;margin-bottom:8px;text-shadow:2px 2px 4px rgba(0,0,0,0.2);word-wrap:break-word;}"
    ".header p{opacity:0.9;font-size:0.95em;}"
    ".nav{background:#f8f9fa;padding:15px;border-bottom:2px solid #e9ecef;display:flex;flex-wrap:wrap;gap:8px;justify-content:center;}"
    ".btn{display:inline-block;padding:14px 20px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;border:none;border-radius:8px;text-decoration:none;font-weight:600;cursor:pointer;transition:all 0.3s;box-shadow:0 4px 15px rgba(102,126,234,0.4);font-size:0.9em;min-height:44px;touch-action:manipulation;}"
    ".btn:active{transform:scale(0.95);box-shadow:0 2px 8px rgba(102,126,234,0.4);}"
    ".btn:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(102,126,234,0.6);}"
    ".btn-success{background:linear-gradient(135deg,#11998e 0%,#38ef7d 100%);box-shadow:0 4px 15px rgba(17,153,142,0.4);}"
    ".btn-danger{background:linear-gradient(135deg,#eb3349 0%,#f45c43 100%);box-shadow:0 4px 15px rgba(235,51,73,0.4);}"
    ".btn-warning{background:linear-gradient(135deg,#f093fb 0%,#f5576c 100%);box-shadow:0 4px 15px rgba(240,147,251,0.4);}"
    ".btn-info{background:linear-gradient(135deg,#4facfe 0%,#00f2fe 100%);box-shadow:0 4px 15px rgba(79,172,254,0.4);}"
    ".content{padding:15px;}"
    ".card{background:#fff;border-radius:12px;padding:20px;margin:15px 0;box-shadow:0 4px 15px rgba(0,0,0,0.1);border-left:4px solid #667eea;}"
    ".card h2{color:#333;margin-bottom:12px;font-size:1.5em;word-wrap:break-word;}"
    ".status-card{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;border-left:4px solid #fff;}"
    ".status-card h2{color:#fff;}"
    ".stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin:15px 0;}"
    ".stat-item{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:15px;border-radius:12px;text-align:center;box-shadow:0 4px 15px rgba(102,126,234,0.3);}"
    ".stat-item h3{font-size:2em;margin:8px 0;}"
    ".stat-item p{opacity:0.9;font-size:0.9em;}"
    ".table-wrapper{overflow-x:auto;-webkit-overflow-scrolling:touch;margin:15px 0;}"
    "table{width:100%;min-width:600px;border-collapse:collapse;background:#fff;border-radius:8px;overflow:hidden;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
    "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:12px 10px;text-align:left;font-weight:600;font-size:0.9em;white-space:nowrap;}"
    "td{padding:10px;border-bottom:1px solid #e9ecef;font-size:0.85em;word-wrap:break-word;}"
    "tr:hover{background:#f8f9fa;}"
    "input,select,textarea{width:100%;padding:14px;border:2px solid #e9ecef;border-radius:8px;font-size:16px;margin:8px 0;touch-action:manipulation;-webkit-appearance:none;}"
    "input:focus,select:focus,textarea:focus{outline:none;border-color:#667eea;box-shadow:0 0 0 3px rgba(102,126,234,0.1);}"
    ".form-group{margin:12px 0;}"
    "label{display:block;margin-bottom:6px;color:#333;font-weight:600;font-size:0.95em;}"
    ".search-box{position:relative;margin:15px 0;}"
    ".search-box input{padding-left:45px;font-size:16px;}"
    ".search-box::before{content:'🔍';position:absolute;left:15px;top:18px;font-size:18px;z-index:1;}"
    ".badge{display:inline-block;padding:6px 12px;border-radius:20px;font-size:0.8em;font-weight:600;}"
    ".badge-success{background:#28a745;color:#fff;}"
    ".badge-danger{background:#dc3545;color:#fff;}"
    ".badge-warning{background:#ffc107;color:#333;}"
    ".alert{padding:12px;border-radius:8px;margin:12px 0;border-left:4px solid;font-size:0.9em;}"
    ".alert-info{background:#d1ecf1;border-color:#0c5460;color:#0c5460;}"
    ".alert-success{background:#d4edda;border-color:#155724;color:#155724;}"
    ".alert-warning{background:#fff3cd;border-color:#856404;color:#856404;}"
    ".modal{display:none;position:fixed;z-index:1000;left:0;top:0;width:100%;height:100%;background:rgba(0,0,0,0.5);}"
    ".modal-content{background:#fff;margin:5% auto;padding:20px;border-radius:12px;width:95%;max-width:500px;box-shadow:0 10px 40px rgba(0,0,0,0.3);max-height:90vh;overflow-y:auto;}"
    ".close{color:#aaa;float:right;font-size:28px;font-weight:bold;cursor:pointer;line-height:20px;}"
    ".close:hover,.close:active{color:#000;}"
    ".pulse{animation:pulse 2s infinite;}"
    "@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.5;}}"
    ".loading{text-align:center;padding:20px;}"
    ".spinner{border:4px solid #f3f3f3;border-top:4px solid #667eea;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;margin:20px auto;}"
    "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}"
    "@media(max-width:768px){"
    "body{padding:5px;}"
    ".container{border-radius:10px;}"
    ".header{padding:15px;}"
    ".header h1{font-size:1.5em;}"
    ".header p{font-size:0.85em;}"
    ".nav{padding:10px;gap:6px;}"
    ".btn{padding:12px 16px;font-size:0.85em;min-width:120px;flex:1 1 auto;}"
    ".content{padding:10px;}"
    ".card{padding:15px;margin:10px 0;}"
    ".card h2{font-size:1.3em;}"
    ".stat-grid{grid-template-columns:repeat(2,1fr);gap:10px;}"
    ".stat-item{padding:12px;}"
    ".stat-item h3{font-size:1.8em;}"
    ".stat-item p{font-size:0.85em;}"
    "th,td{padding:8px 6px;font-size:0.8em;}"
    "table{min-width:700px;}"
    ".table-wrapper{margin:10px -10px;padding:0 10px;}"
    "input,select,textarea{font-size:16px;padding:12px;}"
    "label{font-size:0.9em;}"
    ".alert{font-size:0.85em;padding:10px;}"
    ".modal-content{width:98%;margin:2% auto;padding:15px;}"
    "}"
    "@media(max-width:480px){"
    ".header h1{font-size:1.3em;}"
    ".btn{font-size:0.8em;padding:10px 12px;min-width:100px;}"
    ".stat-grid{grid-template-columns:1fr;}"
    ".card{padding:12px;}"
    "th,td{padding:6px 4px;font-size:0.75em;}"
    "table{min-width:650px;}"
    "}"
    "</style></head><body>";
}

String htmlFooter(bool autoRefresh = true) { 
  String script = "<script>"
    "function confirmAction(msg){return confirm(msg);}"
    "function showModal(id){document.getElementById(id).style.display='block';}"
    "function hideModal(id){document.getElementById(id).style.display='none';}"
    "window.onclick=function(e){if(e.target.classList.contains('modal')){e.target.style.display='none';}}";
  if (autoRefresh) {
    script += "// Auto-refresh page every second"
      "setTimeout(function(){location.reload();},1000);";
  }
  script += "</script></body></html>";
  return script;
}

void handleRoot() {
  String page = htmlHeader("Dashboard");
  page += "<div class='container'>";
  page += "<div class='header'><h1>🔐 RFID Access Control</h1><p>Machine Access Management System</p></div>";
  page += "<div class='nav'>";
  page += "<a class='btn' href='/'>🏠 Dashboard</a>";
  page += "<a class='btn btn-success' href='/users'>👥 Users</a>";
  page += "<a class='btn btn-info' href='/logs'>📋 Logs</a>";
  page += "<a class='btn btn-warning' href='/stats'>📊 Statistics</a>";
  page += "<a class='btn' href='/add-user'>➕ Add User</a>";
  page += "<a class='btn' href='/time'>⏰ Set Time</a>";
  page += "</div>";
  page += "<div class='content'>";
  
  // Status Card
  page += "<div class='card status-card'>";
  page += "<h2>⚡ System Status</h2>";
  if (currentSession.active) {
    page += "<p style='font-size:1.3em;margin:10px 0;'><strong>Machine:</strong> <span class='badge badge-success pulse'>ON ⚡</span></p>";
    page += "<p style='font-size:1.1em;'><strong>Relay Status:</strong> <span style='color:#90EE90;font-weight:bold;'>ACTIVE - Machine Running</span></p>";
    page += "<p style='font-size:1.1em;'><strong>Current User:</strong> " + currentSession.roll + " - " + currentSession.name + "</p>";
    page += "<p style='font-size:1.1em;'><strong>UID:</strong> " + currentSession.uid + "</p>";
    page += "<p style='font-size:1.1em;'><strong>Started:</strong> " + formatDateTime(currentSession.startTime) + "</p>";
    unsigned long elapsed = (millis() - currentSession.startMillis) / 1000;
    page += "<p style='font-size:1.1em;'><strong>Duration:</strong> " + String(elapsed) + " seconds</p>";
  } else {
    RTCDateTime now;
    if (rtcRead(now)) {
      page += "<p style='font-size:1.3em;margin:10px 0;'><strong>Machine:</strong> <span class='badge badge-danger'>OFF</span></p>";
      page += "<p style='font-size:1.1em;'><strong>Relay Status:</strong> <span style='color:#ff6b6b;font-weight:bold;'>INACTIVE - Machine Stopped</span></p>";
      page += "<p style='font-size:1.1em;'><strong>RTC Time:</strong> " + formatDateTime(now) + "</p>";
      page += "<p style='font-size:0.9em;opacity:0.8;margin-top:10px;'>💡 Tap a registered RFID card to start the machine</p>";
    } else {
      page += "<p style='font-size:1.3em;margin:10px 0;'><strong>Machine:</strong> <span class='badge badge-danger'>OFF</span></p>";
      page += "<p style='font-size:1.1em;'><strong>Relay Status:</strong> <span style='color:#ff6b6b;font-weight:bold;'>INACTIVE</span></p>";
      page += "<p style='font-size:1.1em;'><strong>RTC:</strong> Not available</p>";
    }
  }
  page += "</div>";
  
  // Quick Stats
  page += "<div class='stat-grid'>";
  page += "<div class='stat-item'><p>Total Users</p><h3>" + String(users.size()) + "</h3></div>";
  page += "<div class='stat-item'><p>Total Logs</p><h3>" + String(logs.size()) + "</h3></div>";
  page += "<div class='stat-item'><p>EEPROM Usage</p><h3>" + String((users.size() * 100) / MAX_USERS) + "%</h3></div>";
  page += "<div class='stat-item'><p>Logs Usage</p><h3>" + String((logs.size() * 100) / MAX_LOGS) + "%</h3></div>";
  page += "</div>";
  
  page += "</div></div>";
  page += htmlFooter(true); // Auto-refresh enabled for dashboard
  server.send(200, "text/html", page);
}

void handleUsers() {
  String page = htmlHeader("Users Management");
  page += "<div class='container'>";
  page += "<div class='header'><h1>👥 Registered Users</h1><p>Manage RFID card access</p></div>";
  page += "<div class='nav'>";
  page += "<a class='btn' href='/'>🏠 Dashboard</a>";
  page += "<a class='btn btn-success' href='/add-user'>➕ Add User</a>";
  page += "<a class='btn btn-info' href='/export/users'>📥 Export CSV</a>";
  page += "</div>";
  page += "<div class='content'>";
  
  // Search box
  page += "<div class='search-box'><input type='text' id='searchInput' placeholder='Search by UID, Roll, or Name...' onkeyup='filterTable()'></div>";
  
  page += "<div class='card'><h2>Users (" + String(users.size()) + ")</h2>";
  page += "<div class='table-wrapper'><table id='usersTable'><tr><th>#</th><th>UID</th><th>Roll</th><th>Name</th><th>Actions</th></tr>";
  for (size_t i = 0; i < users.size(); i++) {
    page += "<tr><td>" + String(i+1) + "</td><td><code>" + users[i].uid + "</code></td><td>" + users[i].roll + "</td><td>" + users[i].name + "</td>";
    page += "<td><a class='btn btn-warning' href='/edit-user?uid=" + users[i].uid + "'>✏️ Edit</a> ";
    page += "<a class='btn btn-danger' href='/delete-user?uid=" + users[i].uid + "' onclick=\"return confirmAction('Delete " + users[i].name + " (" + users[i].roll + ")?');\">🗑️ Delete</a></td>";
    page += "</tr>";
  }
  page += "</table></div></div>";
  
  page += "<script>function filterTable(){var input=document.getElementById('searchInput');var filter=input.value.toUpperCase();var table=document.getElementById('usersTable');var tr=table.getElementsByTagName('tr');for(var i=1;i<tr.length;i++){var td=tr[i].getElementsByTagName('td');var txt='';for(var j=0;j<td.length;j++){txt+=td[j].textContent||td[j].innerText;}if(txt.toUpperCase().indexOf(filter)>-1){tr[i].style.display='';}else{tr[i].style.display='none';}}}</script>";
  
  page += "</div></div>";
  page += htmlFooter(true); // Auto-refresh enabled for users list
  server.send(200, "text/html", page);
}

void handleAddUserForm() {
  String page = htmlHeader("Add New User");
  page += "<div class='container'>";
  page += "<div class='header'><h1>➕ Add New RFID User</h1><p>Register a new access card</p></div>";
  page += "<div class='nav'><a class='btn' href='/users'>← Back to Users</a></div>";
  page += "<div class='content'>";
  page += "<div class='card'>";
  page += "<div class='alert alert-info'><strong>💡 Tip:</strong> Tap an RFID card now — the UID will appear automatically below!</div>";
  page += "<form method='POST' action='/add-user'>";
  page += "<div class='form-group'><label>UID (hex):</label><input id='uidField' name='uid' required placeholder='Tap card to auto-fill' readonly style='background:#f8f9fa;'></div>";
  page += "<div class='form-group'><label>Roll/ID:</label><input name='roll' required placeholder='Enter roll number or ID'></div>";
  page += "<div class='form-group'><label>Name:</label><input name='name' required placeholder='Enter full name'></div>";
  page += "<div class='form-group'><button class='btn btn-success' type='submit' style='width:100%;font-size:1.2em;padding:15px;'>➕ Add User</button></div>";
  page += "</form></div></div></div>";
  page += "<script>"
    "let lastSeenUID='';"
    "function pollUID(){"
    "fetch('/api/read-uid').then(r=>r.json()).then(data=>{"
    "if(data.uid&&data.uid.length>0&&data.uid!==lastSeenUID){"
    "lastSeenUID=data.uid;"
    "document.getElementById('uidField').value=data.uid;"
    "document.getElementById('uidField').style.background='#d4edda';"
    "setTimeout(()=>{document.getElementById('uidField').style.background='#f8f9fa';},1000);"
    "}}).catch(e=>console.error('Poll error:',e));}"
    "setInterval(pollUID,500);pollUID();"
    "</script>";
  page += htmlFooter(false); // No auto-refresh for forms
  server.send(200, "text/html", page);
}

void handleEditUserForm() {
  if (!server.hasArg("uid")) {
    server.send(400, "text/plain", "Missing UID parameter");
    return;
  }
  String uid = server.arg("uid");
  int idx = findUserIndexByUID(uid);
  if (idx < 0) {
    server.send(404, "text/plain", "User not found");
    return;
  }
  
  String page = htmlHeader("Edit User");
  page += "<div class='container'>";
  page += "<div class='header'><h1>✏️ Edit User</h1><p>Update user information</p></div>";
  page += "<div class='nav'><a class='btn' href='/users'>← Back to Users</a></div>";
  page += "<div class='content'>";
  page += "<div class='card'>";
  page += "<form method='POST' action='/edit-user'>";
  page += "<input type='hidden' name='uid' value='" + users[idx].uid + "'>";
  page += "<div class='form-group'><label>UID (hex):</label><input value='" + users[idx].uid + "' readonly style='background:#f8f9fa;'><small style='color:#666;'>UID cannot be changed</small></div>";
  page += "<div class='form-group'><label>Roll/ID:</label><input name='roll' value='" + users[idx].roll + "' required></div>";
  page += "<div class='form-group'><label>Name:</label><input name='name' value='" + users[idx].name + "' required></div>";
  page += "<div class='form-group'><button class='btn btn-success' type='submit' style='width:100%;font-size:1.2em;padding:15px;'>💾 Save Changes</button></div>";
  page += "</form></div></div></div>";
  page += htmlFooter(false); // No auto-refresh for forms
  server.send(200, "text/html", page);
}

void handleAddUserPost() {
  if (!server.hasArg("uid") || !server.hasArg("roll") || !server.hasArg("name")) {
    server.send(400, "text/plain", "Missing fields");
    return;
  }
  String uid = server.arg("uid"); uid.trim();
  String roll = server.arg("roll"); roll.trim();
  String name = server.arg("name"); name.trim();

  if (uid.length() == 0 || roll.length() == 0 || name.length() == 0) {
    server.send(400, "text/plain", "Invalid input");
    return;
  }
  if (findUserIndexByUID(uid) >= 0) {
    server.send(409, "text/plain", "UID already exists");
    return;
  }
  if (users.size() >= MAX_USERS) {
    server.send(507, "text/plain", "Maximum users reached");
    return;
  }

  User u{uid, roll, name, millis()};
  users.push_back(u);
  saveAllUsersToEEPROM();
  
  server.sendHeader("Location", "/users");
  server.send(303);
}

void handleEditUserPost() {
  if (!server.hasArg("uid") || !server.hasArg("roll") || !server.hasArg("name")) {
    server.send(400, "text/plain", "Missing fields");
    return;
  }
  String uid = server.arg("uid"); uid.trim();
  String roll = server.arg("roll"); roll.trim();
  String name = server.arg("name"); name.trim();
  
  int idx = findUserIndexByUID(uid);
  if (idx < 0) {
    server.send(404, "text/plain", "User not found");
    return;
  }
  
  users[idx].roll = roll;
  users[idx].name = name;
  saveAllUsersToEEPROM();
  
  server.sendHeader("Location", "/users");
  server.send(303);
}

void handleDeleteUser() {
  if (!server.hasArg("uid")) {
    server.send(400, "text/plain", "Missing UID parameter");
    return;
  }
  
  String uidToDelete = server.arg("uid");
  int idx = findUserIndexByUID(uidToDelete);
  if (idx < 0) {
    server.send(404, "text/plain", "User not found");
    return;
  }
  
  users.erase(users.begin() + idx);
  saveAllUsersToEEPROM();
  
  server.sendHeader("Location", "/users");
  server.send(302);
}

void handleLogs() {
  String page = htmlHeader("Usage Logs");
  page += "<div class='container'>";
  page += "<div class='header'><h1>📋 Usage Logs</h1><p>Session history and access records</p></div>";
  page += "<div class='nav'>";
  page += "<a class='btn' href='/'>🏠 Dashboard</a>";
  page += "<a class='btn btn-info' href='/export/logs'>📥 Export CSV</a>";
  page += "<a class='btn btn-danger' href='/clear-logs' onclick=\"return confirmAction('Clear ALL logs? This cannot be undone!');\">🗑️ Clear All</a>";
  page += "</div>";
  page += "<div class='content'>";
  
  page += "<div class='card'><h2>Recent Sessions (" + String(logs.size()) + " total)</h2>";
  page += "<div class='table-wrapper' style='max-height:600px;overflow-y:auto;'>";
  page += "<table><tr><th>Start Time</th><th>End Time</th><th>Duration</th><th>UID</th><th>Roll</th><th>Name</th><th>Action</th></tr>";
  
  // Show logs in reverse order (newest first)
  int shown = 0;
  const int MAX_SHOW = 200;
  for (int i = logs.size() - 1; i >= 0 && shown < MAX_SHOW; i--) {
    String duration = String(logs[i].durationSec) + "s";
    if (logs[i].durationSec >= 60) {
      duration = String(logs[i].durationSec / 60) + "m " + String(logs[i].durationSec % 60) + "s";
    }
    page += "<tr><td>" + logs[i].startStr + "</td><td>" + logs[i].endStr + "</td><td>" + duration + "</td>";
    page += "<td><code>" + logs[i].uid + "</code></td><td>" + logs[i].roll + "</td><td>" + logs[i].name + "</td>";
    page += "<td><a class='btn btn-danger' href='/delete-log?index=" + String(i) + "' onclick=\"return confirmAction('Delete this log entry?');\">🗑️</a></td></tr>";
    shown++;
  }
  page += "</table></div></div>";
  page += "</div></div>";
  page += htmlFooter(true); // Auto-refresh enabled for logs
  server.send(200, "text/html", page);
}

void handleDeleteLog() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }
  int index = server.arg("index").toInt();
  if (index < 0 || index >= (int)logs.size()) {
    server.send(404, "text/plain", "Log not found");
    return;
  }
  
  logs.erase(logs.begin() + index);
  saveAllLogsToEEPROM();
  
  server.sendHeader("Location", "/logs");
  server.send(302);
}

void saveAllLogsToEEPROM() {
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt(PREF_LOG_COUNT, logs.size());
  for (size_t i = 0; i < logs.size(); i++) {
    saveLogToEEPROM(logs[i], i);
  }
  preferences.end();
  Serial.println("All logs saved to EEPROM");
}

void handleClearLogs() {
  logs.clear();
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt(PREF_LOG_COUNT, 0);
  // Clear all log keys
  for (int i = 0; i < MAX_LOGS; i++) {
    String startKey = "log_start_" + String(i);
    String endKey = "log_end_" + String(i);
    String durKey = "log_dur_" + String(i);
    String uidKey = "log_uid_" + String(i);
    String rollKey = "log_roll_" + String(i);
    String nameKey = "log_name_" + String(i);
    preferences.remove(startKey.c_str());
    preferences.remove(endKey.c_str());
    preferences.remove(durKey.c_str());
    preferences.remove(uidKey.c_str());
    preferences.remove(rollKey.c_str());
    preferences.remove(nameKey.c_str());
  }
  preferences.end();
  Serial.println("All logs cleared from EEPROM");
  
  server.sendHeader("Location", "/logs");
  server.send(302);
}

void handleStats() {
  String page = htmlHeader("Statistics");
  page += "<div class='container'>";
  page += "<div class='header'><h1>📊 Statistics</h1><p>System analytics and insights</p></div>";
  page += "<div class='nav'><a class='btn' href='/'>🏠 Dashboard</a></div>";
  page += "<div class='content'>";
  
  // Calculate statistics
  unsigned long totalDuration = 0;
  unsigned long avgDuration = 0;
  String mostActiveUser = "N/A";
  int mostActiveCount = 0;
  
  if (logs.size() > 0) {
    for (const auto& log : logs) {
      totalDuration += log.durationSec;
    }
    avgDuration = totalDuration / logs.size();
    
    // Find most active user
    std::vector<String> uids;
    std::vector<int> counts;
    for (const auto& log : logs) {
      bool found = false;
      for (size_t i = 0; i < uids.size(); i++) {
        if (uids[i] == log.uid) {
          counts[i]++;
          found = true;
          break;
        }
      }
      if (!found) {
        uids.push_back(log.uid);
        counts.push_back(1);
      }
    }
    for (size_t i = 0; i < counts.size(); i++) {
      if (counts[i] > mostActiveCount) {
        mostActiveCount = counts[i];
        int idx = findUserIndexByUID(uids[i]);
        if (idx >= 0) {
          mostActiveUser = users[idx].name + " (" + users[idx].roll + ")";
        }
      }
    }
  }
  
  page += "<div class='stat-grid'>";
  page += "<div class='stat-item'><p>Total Sessions</p><h3>" + String(logs.size()) + "</h3></div>";
  page += "<div class='stat-item'><p>Total Usage Time</p><h3>" + String(totalDuration / 3600) + "h " + String((totalDuration % 3600) / 60) + "m</h3></div>";
  page += "<div class='stat-item'><p>Average Session</p><h3>" + String(avgDuration) + "s</h3></div>";
  page += "<div class='stat-item'><p>Registered Users</p><h3>" + String(users.size()) + "</h3></div>";
  page += "</div>";
  
  page += "<div class='card'><h2>📈 Insights</h2>";
  page += "<p><strong>Most Active User:</strong> " + mostActiveUser + " (" + String(mostActiveCount) + " sessions)</p>";
  page += "<p><strong>EEPROM Capacity:</strong> " + String(MAX_USERS) + " users, " + String(MAX_LOGS) + " logs</p>";
  page += "<p><strong>Current Usage:</strong> " + String((users.size() * 100) / MAX_USERS) + "% users, " + String((logs.size() * 100) / MAX_LOGS) + "% logs</p>";
  page += "</div>";
  
  page += "</div></div>";
  page += htmlFooter(true); // Auto-refresh enabled for stats
  server.send(200, "text/html", page);
}

void handleExportUsers() {
  String csv = "uid,roll,name,created_at\n";
  for (const auto& u : users) {
    csv += u.uid + "," + u.roll + "," + u.name + "," + String(u.createdAt) + "\n";
  }
  server.send(200, "text/csv", csv);
  server.sendHeader("Content-Disposition", "attachment; filename=users.csv");
}

void handleExportLogs() {
  String csv = "start,end,duration_seconds,uid,roll,name\n";
  for (const auto& log : logs) {
    csv += log.startStr + "," + log.endStr + "," + String(log.durationSec) + "," + log.uid + "," + log.roll + "," + log.name + "\n";
  }
  server.send(200, "text/csv", csv);
  server.sendHeader("Content-Disposition", "attachment; filename=logs.csv");
}

void handleTimeForm() {
  String page = htmlHeader("Set RTC Time");
  page += "<div class='container'>";
  page += "<div class='header'><h1>⏰ Set RTC Time</h1><p>Configure DS3231 Real-Time Clock</p></div>";
  page += "<div class='nav'><a class='btn' href='/'>🏠 Dashboard</a></div>";
  page += "<div class='content'>";
  
  RTCDateTime now;
  String nowStr = rtcRead(now) ? formatDateTime(now) : String("RTC not available");
  
  page += "<div class='card'><h2>Current RTC Time</h2><p style='font-size:1.5em;color:#667eea;'>" + nowStr + "</p></div>";
  page += "<div class='card'>";
  page += "<form method='POST' action='/time'>";
  page += "<div class='form-group'><label>Year:</label><input type='number' name='year' value='2025' min='2000' max='2099' required></div>";
  page += "<div class='form-group'><label>Month:</label><input type='number' name='month' value='1' min='1' max='12' required></div>";
  page += "<div class='form-group'><label>Day:</label><input type='number' name='day' value='1' min='1' max='31' required></div>";
  page += "<div class='form-group'><label>Hour (24h):</label><input type='number' name='hour' value='0' min='0' max='23' required></div>";
  page += "<div class='form-group'><label>Minute:</label><input type='number' name='minute' value='0' min='0' max='59' required></div>";
  page += "<div class='form-group'><label>Second:</label><input type='number' name='second' value='0' min='0' max='59' required></div>";
  page += "<div class='form-group'><button class='btn btn-success' type='submit' style='width:100%;font-size:1.2em;padding:15px;'>💾 Set Time</button></div>";
  page += "</form></div></div></div>";
  page += htmlFooter(false); // No auto-refresh for forms
  server.send(200, "text/html", page);
}

void handleTimePost() {
  if (!server.hasArg("year") || !server.hasArg("month") || !server.hasArg("day")
   || !server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("second")) {
    server.send(400, "text/plain", "Missing fields");
    return;
  }
  RTCDateTime dt;
  dt.year   = server.arg("year").toInt();
  dt.month  = server.arg("month").toInt();
  dt.day    = server.arg("day").toInt();
  dt.hour   = server.arg("hour").toInt();
  dt.minute = server.arg("minute").toInt();
  dt.second = server.arg("second").toInt();

  bool ok = rtcWrite(dt);
  if (!ok) { server.send(500, "text/plain", "Failed to set RTC"); return; }
  server.sendHeader("Location", "/time");
  server.send(303);
}

void handleReadUID() {
  String json = "{\"uid\":\"";
  json += lastScannedUID;
  json += "\",\"timestamp\":";
  json += lastScannedTime;
  json += "}";
  server.send(200, "application/json", json);
}

// ------------------------ Setup/Loop ------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Initialize relay pin
  Serial.println("\n=== RELAY INITIALIZATION ===");
  pinMode(PIN_RELAY, OUTPUT);
  Serial.println("Relay pin " + String(PIN_RELAY) + " set as OUTPUT");
  
  // Set initial state (OFF)
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(PIN_RELAY, LOW); // OFF state for active HIGH
  } else {
    digitalWrite(PIN_RELAY, HIGH); // OFF state for active LOW
  }
  relayState = false;
  
  Serial.println("Relay configuration: " + String(RELAY_ACTIVE_HIGH ? "ACTIVE HIGH" : "ACTIVE LOW"));
  Serial.println("Initial state: OFF");
  Serial.println("Pin " + String(PIN_RELAY) + " = " + String(digitalRead(PIN_RELAY)));
  delay(200); // Allow relay to settle
  
  // Test relay on startup
  Serial.println("\nPerforming relay test...");
  testRelay();
  Serial.println("=== RELAY INITIALIZATION COMPLETE ===\n");
  
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  // Initialize buzzer pin
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  noTone(PIN_BUZZER);
  Serial.println("Buzzer pin " + String(PIN_BUZZER) + " initialized");

  // SPI for RFID only (no SD card)
  SPI.begin(); // SCK:18, MISO:19, MOSI:23

  // Load data from EEPROM
  loadUsersFromEEPROM();
  loadLogsFromEEPROM();
  Serial.println("Data loaded from EEPROM");

  // I2C (RTC)
  Wire.begin(I2C_SDA, I2C_SCL);

  // Scan I2C bus
  Serial.println("Scanning I2C bus...");
  int i2c_count = 0;
  for (byte addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  I2C device at 0x"); 
      Serial.print(addr, HEX); 
      Serial.print(" (");
      if (addr == 0x27 || addr == 0x3F) Serial.print("LCD");
      else if (addr == 0x68) Serial.print("DS3231 RTC");
      else Serial.print("Unknown");
      Serial.println(")");
      i2c_count++;
    }
  }
  Serial.print("Total I2C devices found: "); Serial.println(i2c_count);

  // init LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("RFID System Ready");

  // RFID
  rfid.PCD_Init();
  Serial.println("RFID init OK");

  // WiFi SoftAP
  IPAddress local_ip(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPdisconnect(true);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress IP = WiFi.softAPIP();
  dnsServer.start(53, "*", local_ip);
  Serial.print("SoftAP start: "); Serial.println(apOk ? "OK" : "FAIL");
  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP IP: "); Serial.println(IP);

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/users", HTTP_GET, handleUsers);
  server.on("/add-user", HTTP_GET, handleAddUserForm);
  server.on("/add-user", HTTP_POST, handleAddUserPost);
  server.on("/edit-user", HTTP_GET, handleEditUserForm);
  server.on("/edit-user", HTTP_POST, handleEditUserPost);
  server.on("/delete-user", HTTP_GET, handleDeleteUser);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/delete-log", HTTP_GET, handleDeleteLog);
  server.on("/clear-logs", HTTP_GET, handleClearLogs);
  server.on("/stats", HTTP_GET, handleStats);
  server.on("/export/users", HTTP_GET, handleExportUsers);
  server.on("/export/logs", HTTP_GET, handleExportLogs);
  server.on("/time", HTTP_GET, handleTimeForm);
  server.on("/time", HTTP_POST, handleTimePost);
  server.on("/api/read-uid", HTTP_GET, handleReadUID);

  server.begin();
  Serial.println("Server started at http://192.168.4.1/");

  server.onNotFound([](){
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });
  
  // System ready beep
  delay(500); // Wait for everything to settle
  buzzerSystemReady();
  Serial.println("=== System Ready ===");
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();

  // Button handling (debounced) to end session
  static unsigned long lastDebounce = 0;
  static int lastStable = HIGH;
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastStable) {
    lastDebounce = millis();
    lastStable = reading;
  }
  if ((millis() - lastDebounce) > 50) {
    if (reading == LOW && currentSession.active) {
      buzzerButtonPress(); // Button press feedback
      
      RTCDateTime endTime;
      bool okRTC = rtcRead(endTime);
      unsigned long durationSec = (millis() - currentSession.startMillis) / 1000UL;

      LogEntry logEntry;
      logEntry.startStr = formatDateTime(currentSession.startTime);
      logEntry.endStr = okRTC ? formatDateTime(endTime) : String("RTC_ERROR");
      logEntry.durationSec = durationSec;
      logEntry.uid = currentSession.uid;
      logEntry.roll = currentSession.roll;
      logEntry.name = currentSession.name;
      
      appendLogToEEPROM(logEntry);
      Serial.println("Session logged: " + logEntry.startStr + " -> " + logEntry.endStr + " (" + String(durationSec) + "s)");
      
      currentSession = { false, "", "", "", {0,0,0,0,0,0}, 0 };
      
      // Display on LCD: "Machine: OFF" + duration in HH:MM:SS format
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Machine: OFF");
      lcd.setCursor(0, 1);
      String durationStr = "Dur: " + formatDurationLCD(durationSec);
      // Pad to 16 chars to clear line
      while (durationStr.length() < 16) durationStr += " ";
      lcd.print(durationStr);
      
      Serial.println("Session ended. Duration: " + formatDuration(durationSec));
      
      machineOff();
      buzzerSessionEnd(); // Session end beep
      delay(300); // prevent bounce retrigger
    }
  }

  // RFID to start session
  String uid = uidHexFromRFID();
  if (uid.length() > 0) {
    lastScannedUID = uid;
    lastScannedTime = millis();
    
    int idx = findUserIndexByUID(uid);
    String formattedUid = "";
    for (size_t i = 0; i < uid.length(); i += 2) {
      if (i + 2 <= uid.length()) {
        if (formattedUid.length() > 0) formattedUid += " ";
        formattedUid += uid.substring(i, i+2);
      }
    }
    if (formattedUid.length() > 12) formattedUid = formattedUid.substring(0, 12);

    lcd.setCursor(0,0);
    lcd.print("                ");
    lcd.setCursor(0,0);
    lcd.print("UID:");
    lcd.setCursor(4,0);
    lcd.print(formattedUid);
    Serial.println("Displaying UID on LCD: " + formattedUid);

    if (idx < 0) {
      // Unknown/Unregistered card
      buzzerCardError(); // Error beep for unknown card
      Serial.println("Unknown UID: " + uid);
      lcd.setCursor(0,1);
      lcd.print("UNKNOWN          ");
    } else {
      // Valid registered card
      buzzerCardSuccess(); // Success beep for valid card
      
      if (!currentSession.active) {
        RTCDateTime now;
        bool okRTC = rtcRead(now);
        currentSession.active = true;
        currentSession.uid = users[idx].uid;
        currentSession.roll = users[idx].roll;
        currentSession.name = users[idx].name;
        currentSession.startTime = okRTC ? now : RTCDateTime{2000,1,1,0,0,0};
        currentSession.startMillis = millis();
        Serial.println("\n*** SESSION STARTING ***");
        Serial.println("User: " + currentSession.roll + " - " + currentSession.name);
        Serial.println("UID: " + currentSession.uid);
        
        // Turn on relay FIRST before LCD update
        Serial.println("\n>>> ACTIVATING RELAY NOW <<<");
        machineOn();
        delay(100); // Delay to ensure relay activates and settles
        Serial.println(">>> RELAY ACTIVATION COMPLETE <<<\n");
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Machine: ON");
        lcd.setCursor(0, 1);
        String displayName = currentSession.roll.length() > 0 ? currentSession.roll : currentSession.name;
        while (displayName.length() < 16) displayName += " ";
        lcd.print(displayName);
        
        buzzerSessionStart(); // Session start beep
        Serial.println("=== RELAY ACTIVATED ===");
        Serial.println("User: " + currentSession.roll + " - " + currentSession.name);
        Serial.println("Relay Pin " + String(PIN_RELAY) + " state: " + String(digitalRead(PIN_RELAY)));
      } else {
        Serial.println("Session already active; press button to end.");
      }
    }
    delay(200);
  }
}
