# 🔌 Complete Wiring Guide

## Quick Reference Pin Table

| Component | Signal | ESP32 Pin | Notes |
|-----------|--------|-----------|------|
| **MFRC522 RFID** | SS (SDA) | GPIO 21 | SPI Chip Select |
| | SCK | GPIO 18 | SPI Clock |
| | MOSI | GPIO 23 | SPI Master Out |
| | MISO | GPIO 19 | SPI Master In |
| | RST | GPIO 22 | Reset pin |
| | 3.3V | 3.3V | Power (NOT 5V!) |
| | GND | GND | Ground |
| **Relay Module** | IN | GPIO 26 | Control signal |
| | VCC | 5V | Power supply |
| | GND | GND | Ground |
| | NO/NC/COM | Machine | Load connection |
| **16x2 LCD (I2C)** | SDA | GPIO 32 | I2C Data |
| | SCL | GPIO 33 | I2C Clock |
| | VCC | 5V | Power |
| | GND | GND | Ground |
| **DS3231 RTC** | SDA | GPIO 32 | I2C Data (shared) |
| | SCL | GPIO 33 | I2C Clock (shared) |
| | VCC | 5V | Power |
| | GND | GND | Ground |
| **Passive Buzzer** | Positive (+) | GPIO 25 | PWM signal |
| | Negative (-) | GND | Ground |
| **Push Button** | Terminal 1 | GPIO 27 | Digital input |
| | Terminal 2 | GND | Ground |

## Step-by-Step Wiring Instructions

### Step 1: Power Connections
1. Connect all **GND** pins together (common ground bus)
2. Connect ESP32 **5V** to power rail (for modules needing 5V)
3. Connect ESP32 **3.3V** to MFRC522 VCC only
4. **Important**: MFRC522 requires 3.3V, NOT 5V!

### Step 2: MFRC522 RFID Reader
```
MFRC522 Module → ESP32
───────────────────────
SDA (SS)    → GPIO 21
SCK         → GPIO 18
MOSI        → GPIO 23
MISO        → GPIO 19
RST         → GPIO 22
VCC         → 3.3V (NOT 5V!)
GND         → GND
```

**Verification**: 
- After upload, Serial Monitor should show "RFID init OK"
- Tap a card, you should see UID in Serial Monitor

### Step 3: Relay Module
```
Relay Module → ESP32
────────────────────
IN (Signal) → GPIO 26
VCC         → 5V
GND         → GND

Relay Module → Machine/Load
────────────────────────────
NO (Normally Open) → Load Terminal 1
COM (Common)       → Power Source
NC (Normally Closed) → Not used (or Load Terminal 2)
```

**Relay Wiring for Machine**:
- **NO (Normally Open)**: When relay is OFF, circuit is open
- **COM (Common)**: Common terminal
- **NC (Normally Closed)**: When relay is OFF, circuit is closed

**Typical Machine Connection**:
- Connect machine's power wire through relay NO and COM
- When relay activates (OFF in our case), machine power is cut
- When relay deactivates (ON), machine power is restored

**Verification**:
- Listen for relay "click" sound when toggling
- Check relay LED indicator
- Use Serial Monitor to see relay state changes

### Step 4: I2C Devices (LCD + RTC)
```
LCD I2C Module → ESP32
──────────────────────
SDA           → GPIO 32
SCL           → GPIO 33
VCC           → 5V
GND           → GND

DS3231 RTC → ESP32
───────────────────
SDA        → GPIO 32 (shared with LCD)
SCL        → GPIO 33 (shared with LCD)
VCC        → 5V
GND        → GND
```

**I2C Bus Sharing**:
- Both LCD and RTC share the same I2C bus (SDA/SCL)
- They have different addresses (LCD: 0x27/0x3F, RTC: 0x68)
- No conflicts, ESP32 can communicate with both

**Verification**:
- Serial Monitor shows I2C scan results
- LCD should display "RFID System Ready" on boot
- RTC time should be readable via web interface

### Step 5: Passive Buzzer
```
Buzzer → ESP32
──────────────
+ (Positive) → GPIO 25
- (Negative) → GND
```

**Note**: 
- Must be a **passive buzzer** (not active)
- Active buzzers won't work with tone() function
- If unsure, test: passive buzzers need PWM signal, active buzzers just need power

**Verification**:
- System should beep twice on startup
- Should beep when RFID card is tapped
- Should beep when button is pressed

### Step 6: Push Button
```
Push Button → ESP32
───────────────────
Terminal 1  → GPIO 27
Terminal 2  → GND
```

**Button Type**:
- Momentary push button (NO - Normally Open)
- Uses ESP32 internal pull-up resistor
- Pressing button connects GPIO 27 to GND (reads LOW)

**Verification**:
- Button press should trigger session end
- Serial Monitor shows button press events
- Buzzer should click when button pressed

## Complete Wiring Diagram (Text)

```
                    ┌─────────────────────────┐
                    │      ESP32 DevKit       │
                    │                         │
                    │  GPIO 21 ────┐          │
                    │  GPIO 18 ────┤         │
                    │  GPIO 23 ────┤ SPI     │
                    │  GPIO 19 ────┤         │
                    │  GPIO 22 ────┘          │
                    │                         │
                    │  GPIO 26 ───────────────┼───► Relay IN
                    │  GPIO 27 ───────────────┼───► Button
                    │  GPIO 25 ───────────────┼───► Buzzer +
                    │                         │
                    │  GPIO 32 ───────────────┼───► I2C SDA (LCD + RTC)
                    │  GPIO 33 ───────────────┼───► I2C SCL (LCD + RTC)
                    │                         │
                    │  3.3V ──────────────────┼───► MFRC522 VCC
                    │  5V ────────────────────┼───► Relay, LCD, RTC VCC
                    │  GND ───────────────────┼───► All GND
                    └─────────────────────────┘
```

## Power Supply Requirements

### Current Draw
- **ESP32**: ~80-120mA (idle to active)
- **MFRC522**: ~30mA
- **LCD**: ~20mA
- **RTC**: ~2mA
- **Relay Module**: ~70mA (when active)
- **Buzzer**: ~10mA (when active)
- **Total**: ~200-250mA maximum

### Power Supply Options
1. **USB Power** (Recommended)
   - 5V/2A USB adapter
   - Connect to ESP32 USB port
   - Provides stable 5V power

2. **External 5V Supply**
   - 5V/2A DC adapter
   - Connect to ESP32 5V pin (bypass USB)
   - Ensure common ground

3. **Battery Power** (Optional)
   - 5V power bank
   - USB connection
   - Portable operation

## Common Wiring Mistakes

### ❌ Don't Do This:
1. **MFRC522 to 5V**: Will damage the module! Use 3.3V only
2. **Missing GND connections**: All modules must share common ground
3. **Wrong I2C address**: LCD might be 0x3F instead of 0x27
4. **Active buzzer instead of passive**: Won't work with tone() function
5. **Relay active HIGH when it's LOW**: Check relay module type
6. **Button without pull-up**: Use internal pull-up (already in code)

### ✅ Do This:
1. **Use 3.3V for MFRC522**: Check twice before connecting
2. **Common ground**: Connect all GND together
3. **Verify I2C addresses**: Use I2C scanner if needed
4. **Test each component**: Verify one at a time
5. **Check relay type**: Active LOW vs Active HIGH
6. **Use proper power supply**: 5V/2A minimum

## Testing Each Component

### Test Sequence

1. **Power On**
   - LED on ESP32 should light up
   - Serial Monitor should show initialization

2. **RFID Reader**
   - Serial Monitor: "RFID init OK"
   - Tap card: See UID in Serial Monitor

3. **Relay**
   - Serial Monitor: "Relay test sequence"
   - Listen for relay clicks
   - Check relay LED indicator

4. **LCD Display**
   - Should show "RFID System Ready"
   - Check contrast if needed

5. **RTC**
   - Web interface: Set time
   - Verify time persists after power cycle

6. **Buzzer**
   - Should beep twice on startup
   - Should beep on card tap

7. **Button**
   - Press button during active session
   - Should end session and turn relay ON

8. **WiFi**
   - Connect to "MachineAccess" network
   - Access http://192.168.4.1

## Troubleshooting Wiring Issues

### No Response from RFID
- Check SPI connections (all 5 wires)
- Verify 3.3V power (not 5V!)
- Try different RFID cards
- Check Serial Monitor for errors

### Relay Not Working
- Verify GPIO 26 connection
- Check relay module power (5V)
- Test relay manually (jumper IN to GND)
- Verify relay type (active LOW/HIGH)

### LCD Blank Screen
- Check I2C connections (SDA, SCL)
- Verify I2C address (0x27 or 0x3F)
- Adjust contrast potentiometer
- Check 5V power supply

### I2C Devices Not Found
- Verify SDA/SCL connections
- Check I2C pull-up resistors
- Ensure devices have power
- Use I2C scanner to find addresses

### Buzzer No Sound
- Verify passive buzzer (not active)
- Check GPIO 25 connection
- Test with different frequency
- Check buzzer polarity

## Safety Precautions

⚠️ **Important Safety Notes**:

1. **Power Off Before Wiring**: Always disconnect power before making connections
2. **Check Voltage**: Verify 3.3V vs 5V before connecting
3. **Relay Load**: Ensure relay can handle your machine's current/voltage
4. **Proper Grounding**: Always use common ground
5. **Fuse Protection**: Consider adding fuse for machine power circuit
6. **Insulation**: Cover exposed connections
7. **Heat Management**: Ensure adequate ventilation

## Additional Resources

- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [MFRC522 Datasheet](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf)
- [I2C Communication Guide](https://learn.sparkfun.com/tutorials/i2c/all)
- [Relay Module Guide](https://lastminuteengineers.com/relay-module-arduino-tutorial/)

---

**Need Help?** Check the main README.md troubleshooting section or open an issue on GitHub.
