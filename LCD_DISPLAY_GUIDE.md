# LCD Display Quick Reference

## 16x2 I2C LCD Display - All States

### LCD Specifications
- **Type:** 16x2 Character LCD with I2C backpack
- **I2C Address:** 0x27 (or 0x3F, configurable)
- **Pin Connections:** GPIO 32 (SDA), GPIO 33 (SCL)
- **Rows:** 2
- **Columns:** 16 characters per row

---

## Display States During Operation

### State 0: Startup / Idle (Machine OFF)
```
Row 1: [                ]  (blank or welcome message)
Row 2: [                ]  (blank)
```
- Relay: OFF (GPIO 26 = LOW)
- Status: Waiting for RFID card

---

### State 1: RFID Card Detected (Session Starting)
```
Row 1: [M][a][c][h][i][n][e][:][ ][O][N][ ][ ][ ][ ][ ]
Row 2: [R][O][L][L][_][N][A][M][E][ ][ ][ ][ ][ ][ ][ ]
       ^16 characters, padded with spaces^
```
**Timeline:**
- User taps RFID card
- UID validated (card registered in system)
- LCD clears immediately
- Line 1: Displays "Machine: ON" (11 chars + 5 spaces = 16)
- Line 2: User's roll number (or name if roll empty), padded to 16 chars
- Relay: ON (GPIO 26 = HIGH)
- Duration: LCD updates every loop, but text stays same

**Example:**
```
Machine: ON     
CSE001          
```
or
```
Machine: ON     
John Doe        
```

---

### State 2: Session Active (Machine Running)
```
Row 1: [M][a][c][h][i][n][e][:][ ][O][N][ ][ ][ ][ ][ ]
Row 2: [R][O][L][L][_][N][A][M][E][ ][ ][ ][ ][ ][ ][ ]
```
**Timeline:**
- Same as State 1
- User operates machine
- No change to LCD display during operation

**Duration:** Can vary from seconds to hours
**Relay State:** Remains ON

---

### State 3: Button Pressed (Session Ending)
```
Row 1: [M][a][c][h][i][n][e][:][ ][O][F][F][ ][ ][ ][ ]
Row 2: [D][u][r][a][t][i][o][n][:][ ][0][0][5][s][ ][ ]
       ^16 characters, padded with spaces^
```
**Timeline:**
- User presses GPIO 27 button (push-to-ground)
- Debounce delay: ~50ms
- RTC reads end time
- Duration calculated from start timestamp
- LCD clears immediately
- Line 1: Displays "Machine: OFF" (12 chars + 4 spaces = 16)
- Line 2: Displays "Duration: XXXs" (duration in seconds, padded to 16 chars)
- Relay: OFF (GPIO 26 = LOW)
- Session logged to SD + EEPROM
- System returns to idle state

**Examples:**
```
Machine: OFF    
Duration: 27s   
```
or for longer sessions:
```
Machine: OFF    
Duration: 3456s 
```

---

## Character Layouts (Detailed)

### Machine ON State (16 chars)
```
Position: 123456789012345678
Content:  Machine: ON [spaces]
Chars:    M a c h i n e : SPC O N SPC SPC SPC SPC SPC
Index:    0 1 2 3 4 5 6 7  8  9 10 11 12 13 14 15
```

### Machine OFF State (16 chars)
```
Position: 123456789012345678
Content:  Machine: OFF[spaces]
Chars:    M a c h i n e : SPC O F F SPC SPC SPC SPC SPC
Index:    0 1 2 3 4 5 6 7  8  9 10 11 12 13 14 15
```

### Duration Line Examples (16 chars)
```
Duration: 5s        → [D u r a t i o n : SPC 5 s SPC SPC ...]
Duration: 123s      → [D u r a t i o n : SPC 1 2 3 s SPC ...]
Duration: 3456s     → [D u r a t i o n : SPC 3 4 5 6 s]
Duration: 123456s   → [D u r a t i o n : 1 2 3 4 5 6 s SPC]
```

---

## Timing Sequence Diagram

```
Timeline                  LCD Row 1              LCD Row 2           Relay
--------                  -----------            -----------         ------
0ms        Idle           [blank]                [blank]             LOW
           
100ms      Card Tap       [blank]                [blank]             LOW (checking)
           
150ms      Valid Card     "Machine: ON   "       "CSE001      "      HIGH
           ├─ Clear LCD
           ├─ Print "Machine: ON"
           ├─ Print user name
           └─ Set relay ON
           
5000ms     Session...     "Machine: ON   "       "CSE001      "      HIGH
           (Running)      (no change)            (no change)         (remains ON)
           
27000ms    Button Press   "Machine: ON   "       "CSE001      "      HIGH (still)
           ├─ Debounce 50ms
           ├─ Log session
           
27100ms    Relay OFF      "Machine: OFF  "       "Duration: 27s "     LOW
           ├─ Clear LCD
           ├─ Print "Machine: OFF"
           ├─ Print duration
           └─ Set relay OFF

27200ms    Idle           "Machine: OFF  "       "Duration: 27s "     LOW (remains)

40000ms    Back to Idle   [blank]                [blank]             LOW
           (Optional: Clear LCD for next card)
```

---

## Code Reference

### Session Start - LCD Update
```cpp
// Display on LCD: Line 1 "Machine: ON", Line 2 "User Name/Roll"
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Machine: ON");
lcd.setCursor(0, 1);
String displayName = currentSession.roll.length() > 0 ? 
                     currentSession.roll : currentSession.name;
// Pad to 16 chars to clear line
while (displayName.length() < 16) displayName += " ";
lcd.print(displayName);

machineOn();  // GPIO 26 HIGH
```

### Session End - LCD Update
```cpp
// Display on LCD: "Machine: OFF" + duration
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Machine: OFF");
lcd.setCursor(0, 1);
String durationStr = "Duration: " + String(durationSec) + "s";
// Pad to 16 chars
while (durationStr.length() < 16) durationStr += " ";
lcd.print(durationStr);

machineOff();  // GPIO 26 LOW
```

---

## Display Troubleshooting

### Problem: LCD Shows Garbage Characters
**Solution:**
- Verify I2C address (check with I2C scanner)
- Update line: `LiquidCrystal_I2C lcd(0x27, 16, 2);` to correct address (usually 0x3F)
- Check SDA/SCL connections (GPIO 32/33)
- Verify 5V backlight power connection

### Problem: LCD Backlight ON but No Text
**Solution:**
- Rotate LCD potentiometer (contrast adjustment)
- Check I2C communication: Monitor Serial for address scan output
- Try: `lcd.init()` in setup followed by `lcd.backlight()`
- Verify `#include <LiquidCrystal_I2C.h>` is present

### Problem: Text Stays After Clear
**Solution:**
- Ensure `displayName` or `durationStr` padded to full 16 chars
- Check: `while (str.length() < 16) str += " ";` is present
- Verify `lcd.clear()` called before printing new content

### Problem: Duration Formatting Wrong
**Solution:**
- Verify `durationSec` calculated: `(millis() - startMillis) / 1000`
- Check: `String(durationSec)` conversion is correct
- Ensure padding works: test with durations 1s, 12s, 123s, 1234s

---

## Testing Script (Pseudo-code)

```
Test 1: Power On
  - Expect: LCD backlight ON, blank display
  - Pass if: Backlight is bright

Test 2: Idle State
  - Wait 5 seconds
  - Expect: LCD still blank (or welcome message)
  - Pass if: No garbage text

Test 3: Card Tap (Registered User)
  - Tap RFID card
  - Expect: LCD Line 1 = "Machine: ON"
  - Expect: LCD Line 2 = User name/roll (padded)
  - Pass if: Both lines display correctly

Test 4: Session Active
  - Wait 30 seconds
  - Expect: LCD unchanged
  - Pass if: Text remains same for entire duration

Test 5: Button Press
  - Press GPIO 27 button
  - Expect: LCD Line 1 = "Machine: OFF"
  - Expect: LCD Line 2 = "Duration: 30s"
  - Pass if: Duration matches approximate elapsed time

Test 6: Return to Idle
  - Wait 5 seconds
  - Expect: LCD clears (or shows welcome)
  - Pass if: Display returns to idle state
```

---

## LCD I2C Module Reference

### Typical I2C Module (PCF8574 Controller)
```
    ┌─────────────────┐
    │ PCF8574 I2C     │
    │ Backpack        │
    └────────┬────────┘
             │
    ┌────────┴────────┐
    │                 │
   GND               +5V
    │
  16-pin Connector (to LCD)
    │
    ├─ RS (Register Select)
    ├─ RW (Read/Write)
    ├─ E  (Enable)
    ├─ D4-D7 (Data lines)
    ├─ A (Anode/Backlight)
    └─ K (Cathode/GND)
```

### I2C Pinout
```
GND  ────────  ESP32 GND
5V   ────────  ESP32 5V (or 3.3V with converter)
SDA  ────────  GPIO 32
SCL  ────────  GPIO 33
```

---

## Quick Start - Setup Code

```cpp
// Initialize I2C and LCD
void setup() {
  Wire.begin(32, 33);  // SDA=32, SCL=33
  lcd.init();
  lcd.backlight();
  lcd.print("Ready!");
}

// Display user login (machine on)
void sessionStart(String roll, String name) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Machine: ON");
  lcd.setCursor(0, 1);
  String display = (roll.length() > 0) ? roll : name;
  while (display.length() < 16) display += " ";
  lcd.print(display);
}

// Display session end (machine off)
void sessionEnd(unsigned long durationSec) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Machine: OFF");
  lcd.setCursor(0, 1);
  String duration = "Duration: " + String(durationSec) + "s";
  while (duration.length() < 16) duration += " ";
  lcd.print(duration);
}
```

---

**Version:** 1.0  
**Last Updated:** January 2025  
**Tested With:** ESP32 DevKit + 16x2 I2C LCD (PCF8574 @ 0x27)
