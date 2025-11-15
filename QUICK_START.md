# 🚀 Quick Start Guide

Get your RFID Access Control System up and running in 15 minutes!

## Prerequisites Checklist

- [ ] ESP32 DevKit board
- [ ] MFRC522 RFID reader module
- [ ] 5V relay module (with BC547 transistor)
- [ ] 16x2 I2C LCD display
- [ ] DS3231 RTC module
- [ ] Passive buzzer
- [ ] Push button
- [ ] RFID cards/tags
- [ ] Jumper wires
- [ ] 5V/2A USB power supply
- [ ] Arduino IDE installed
- [ ] USB cable for ESP32

## 5-Minute Setup

### 1. Install Arduino IDE & ESP32 Support (5 min)

1. Download Arduino IDE: https://www.arduino.cc/en/software
2. Install ESP32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. Install libraries:
   - Tools → Manage Libraries
   - Install: **MFRC522** by GithubCommunity
   - Install: **LiquidCrystal_I2C** by Frank de Brabander

### 2. Wire Components (5 min)

**Essential Connections** (minimum to test):

```
MFRC522:
  SDA → GPIO 21
  SCK → GPIO 18
  MOSI → GPIO 23
  MISO → GPIO 19
  RST → GPIO 22
  VCC → 3.3V (NOT 5V!)
  GND → GND

Relay:
  IN → GPIO 26
  VCC → 5V
  GND → GND

LCD (I2C):
  SDA → GPIO 32
  SCL → GPIO 33
  VCC → 5V
  GND → GND

RTC:
  SDA → GPIO 32 (shared)
  SCL → GPIO 33 (shared)
  VCC → 5V
  GND → GND

Buzzer:
  + → GPIO 25
  - → GND

Button:
  Terminal 1 → GPIO 27
  Terminal 2 → GND
```

### 3. Upload Code (2 min)

1. Open `rfid_v1_co.ino` in Arduino IDE
2. Select board: Tools → Board → ESP32 Dev Module
3. Select port: Tools → Port → (your COM port)
4. Click Upload (→) button
5. Wait for "Done uploading"

### 4. First Test (3 min)

1. Open Serial Monitor (Tools → Serial Monitor, 115200 baud)
2. You should see:
   ```
   === RELAY INITIALIZATION ===
   *** RELAY TEST SEQUENCE STARTING ***
   RFID init OK
   SoftAP start: OK
   Server started at http://192.168.4.1/
   === System Ready ===
   ```
3. LCD should show: "RFID System Ready"
4. Buzzer should beep twice

## First Use

### Connect to WiFi

1. On your phone/computer, scan for WiFi networks
2. Connect to: **MachineAccess**
3. Password: **12345678**

### Access Web Interface

1. Open browser
2. Go to: **http://192.168.4.1**
3. Dashboard should load

### Set Time

1. Click "⏰ Set Time"
2. Enter current date/time
3. Click "Set Time"

### Register First User

1. Click "➕ Add User"
2. **Tap an RFID card** on the reader
3. UID will auto-fill in the form
4. Enter Roll/ID and Name
5. Click "Add User"

### Test System

1. **Tap registered card**:
   - Buzzer beeps
   - Relay turns OFF
   - LCD shows "Machine: OFF" + user name
   - Session starts

2. **Press button**:
   - Buzzer beeps
   - Relay turns ON
   - LCD shows "Machine: ON" + duration
   - Session ends and logs

## Common Issues & Quick Fixes

| Problem | Quick Fix |
|---------|-----------|
| Relay not working | Change `RELAY_ACTIVE_HIGH = true` in code (line 105) |
| RFID not reading | Check MFRC522 is on 3.3V (not 5V!) |
| LCD blank | Check I2C address (try 0x3F instead of 0x27) |
| Can't connect WiFi | Verify SSID: "MachineAccess", password: "12345678" |
| Web not loading | Check you're connected to "MachineAccess" network |
| No buzzer sound | Verify passive buzzer (not active) |

## Next Steps

- ✅ Read full [README.md](README.md) for detailed documentation
- ✅ Check [WIRING_GUIDE.md](WIRING_GUIDE.md) for complete wiring
- ✅ Customize settings in code
- ✅ Add more users
- ✅ Export logs for analysis

## Need Help?

- Check Serial Monitor for error messages
- Review troubleshooting section in README.md
- Verify all connections match wiring guide
- Test each component individually

---

**You're all set! 🎉**

The system is now ready to manage machine access with RFID cards.

