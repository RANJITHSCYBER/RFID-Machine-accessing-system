# RFID Access System - Wiring Guide

## ESP32 Pin Configuration

Your sketch uses the following GPIO pins:

| Component | Signal | ESP32 Pin | Notes |
|-----------|--------|-----------|-------|
| **SD Card Module** | CS | GPIO 4 | Change PIN_SD_CS in code if different |
| | MOSI | GPIO 23 | SPI (usually fixed on ESP32) |
| | MISO | GPIO 19 | SPI (usually fixed on ESP32) |
| | SCK | GPIO 18 | SPI (usually fixed on ESP32) |
| **RFID (MFRC522)** | SS (SDA) | GPIO 21 | MFRC522 chip select |
| | RST | GPIO 22 | MFRC522 reset |
| | MOSI | GPIO 23 | SPI shared with SD |
| | MISO | GPIO 19 | SPI shared with SD |
| | SCK | GPIO 18 | SPI shared with SD |
| **RTC (DS3231)** | SDA | GPIO 32 | I2C data |
| | SCL | GPIO 33 | I2C clock |
| **I2C LCD (16x2)** | SDA | GPIO 32 | I2C data (shared with RTC) |
| | SCL | GPIO 33 | I2C clock (shared with RTC) |
| **Relay** | Signal | GPIO 26 | HIGH to turn ON |
| **Button** | Signal | GPIO 27 | Pull to GND to end session |

---

## DS3231 RTC Module Connection

The DS3231 module typically has 4 pins:

```
DS3231 Module       ESP32
─────────────      ───────
GND       ────────  GND
VCC       ────────  3.3V or 5V (check module spec)
SDA       ────────  GPIO 32
SCL       ────────  GPIO 33
```

### Important Notes:
- **Power:** Most DS3231 modules support both 3.3V and 5V. Use **3.3V** to match ESP32 logic levels.
- **I2C Address:** DS3231 uses I2C address **0x68** (hardcoded, cannot be changed).
- **Shared I2C:** The LCD also uses the same I2C bus (GPIO 32/33). Both devices will work together.
- **Pull-ups:** DS3231 has built-in pull-ups; you typically don't need external ones if only two devices are on the bus.

---

## DS3231 Features & Your Code

Your sketch already:
- Reads time from DS3231 (function `rtcRead()`)
- Sets time on DS3231 (function `rtcWrite()`)
- Uses direct I2C calls (no RTClib dependency)
- Displays time on the web dashboard at `/time` endpoint

### RTC Data Format in Code:

```cpp
struct RTCDateTime {
  uint16_t year;   // 2000-2099
  uint8_t month;   // 1-12
  uint8_t day;     // 1-31
  uint8_t hour;    // 0-23
  uint8_t minute;  // 0-59
  uint8_t second;  // 0-59
};
```

### Reading Time:
```cpp
RTCDateTime now;
if (rtcRead(now)) {
  Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
    now.year, now.month, now.day, now.hour, now.minute, now.second);
}
```

### Setting Time (e.g., to 2025-11-12 14:30:45):
```cpp
RTCDateTime dt{2025, 11, 12, 14, 30, 45};
rtcWrite(dt);
```

---

## I2C Device Addresses

If using multiple I2C devices, verify addresses:

| Device | Address (Hex) | Address (Decimal) |
|--------|---------------|-------------------|
| DS3231 RTC | 0x68 | 104 |
| LCD 16x2 | 0x27 (or 0x3F) | 39 (or 63) |
| AT24C32 EEPROM (on DS3231) | 0x57 | 87 (optional, not used in current code) |

---

## Testing I2C Connection

Upload the following sketch to scan and detect all I2C devices:

```cpp
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(32, 33); // SDA=32, SCL=33
  delay(1000);
  Serial.println("I2C Scanner starting...");
}

void loop() {
  Serial.println("\nScanning I2C bus...");
  int count = 0;
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.println(i, HEX);
      count++;
    }
  }
  if (count == 0) Serial.println("No I2C devices found!");
  delay(5000);
}
```

Expected output:
```
I2C device found at address 0x27  (LCD if present)
I2C device found at address 0x68  (DS3231 RTC)
I2C device found at address 0x57  (AT24C32 EEPROM, optional)
```

---

## Troubleshooting

### RTC not detected (address 0x68 not found)
- Check wiring: ensure SDA and SCL are connected correctly
- Verify power: DS3231 should have 3.3V and GND
- Check for i2c pullup resistors (usually 4.7kΩ between SDA/VCC and SCL/VCC) — most modules have them built-in

### LCD not detected (address 0x27/0x3F not found)
- Verify I2C address: it's usually 0x27 or 0x3F depending on the module
- In the code, change `LiquidCrystal_I2C lcd(0x27, 16, 2);` to `LiquidCrystal_I2C lcd(0x3F, 16, 2);` if needed
- Check wiring: SDA/SCL to correct pins

### DS3231 loses time after power loss
- Battery on the DS3231 module may be dead or missing
- Most DS3231 modules have a CR2032 coin cell battery — replace if needed

---

## Summary

Your system is wired as:
- **SPI Bus (GPIO 18/19/23):** SD card + RFID reader
- **I2C Bus (GPIO 32/33):** DS3231 RTC + LCD + Optional AT24C32 EEPROM
- **Digital I/O:** Relay (GPIO 26), Button (GPIO 27)

All connections follow ESP32 best practices and should work reliably!
