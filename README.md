# 🔐 ESP32 RFID Machine Access Control System

A comprehensive RFID-based access control system for machine management with web interface, real-time monitoring, and EEPROM data storage. Built for ESP32 with support for relay control, LCD display, RTC timekeeping, and passive buzzer feedback.

![ESP32](https://img.shields.io/badge/ESP32-DevKit-00979D?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-IDE-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Pin Connections](#-pin-connections)
- [Circuit Diagram](#-circuit-diagram)
- [Software Requirements](#-software-requirements)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage](#-usage)
- [Web Interface](#-web-interface)
- [System Architecture](#-system-architecture)
- [Troubleshooting](#-troubleshooting)
- [Project Structure](#-project-structure)
- [Contributing](#-contributing)
- [License](#-license)

## ✨ Features

### Core Functionality
- **RFID Card Authentication**: Register and authenticate users via MFRC522 RFID reader
- **Relay Control**: Automatic machine control via relay module (GPIO 26)
- **Session Management**: Track machine usage sessions with start/end times
- **Real-time Monitoring**: Live status updates via web dashboard
- **EEPROM Storage**: All data stored in ESP32 EEPROM (no SD card required)
- **LCD Display**: 16x2 I2C LCD for real-time status display
- **RTC Timekeeping**: DS3231 real-time clock for accurate timestamps
- **Audio Feedback**: Passive buzzer provides audio feedback for all events
- **Web Dashboard**: Modern, mobile-responsive web interface

### Advanced Features
- **Auto-refresh Web Interface**: Dashboard updates every second automatically
- **User Management**: Add, edit, delete users via web interface
- **Session Logging**: Complete session history with duration tracking
- **Statistics Dashboard**: Usage analytics and insights
- **CSV Export**: Export users and logs data
- **Search Functionality**: Search users by UID, roll, or name
- **Mobile-Friendly**: Fully responsive design for smartphones and tablets

## 🔧 Hardware Requirements

| Component | Quantity | Description |
|-----------|----------|-------------|
| ESP32 DevKit | 1 | Main microcontroller (ESP32-WROOM-32) |
| MFRC522 RFID Reader | 1 | RFID card reader module |
| Relay Module (5V) | 1 | Single channel relay with BC547 transistor |
| 16x2 I2C LCD Display | 1 | LCD with I2C backpack (0x27 or 0x3F) |
| DS3231 RTC Module | 1 | Real-time clock module with battery |
| Passive Buzzer | 1 | 5V passive buzzer |
| Push Button | 1 | Momentary push button (NO) |
| RFID Cards/Tags | Multiple | 13.56MHz RFID cards |
| Jumper Wires | - | Male-to-male, male-to-female |
| Breadboard | 1 | Optional, for prototyping |
| Power Supply | 1 | 5V/2A USB power supply or adapter |

### Relay Module Specifications
- **Switching Capacity**: 7250VAC@10A / 30VDC@10A
- **Trigger Current**: 5mA (low current)
- **Working Voltage**: 5V TTL signal
- **Protection**: BC547 transistor, flyback diode
- **LED Indicators**: ON/OFF status indication

## 🔌 Pin Connections

### Complete Wiring Diagram

```
ESP32 DevKit Pinout:
┌─────────────────────────────────────┐
│  ESP32 DevKit                        │
│                                      │
│  GPIO 21 ────► MFRC522 SS (SDA)     │
│  GPIO 22 ────► MFRC522 RST           │
│  GPIO 26 ────► Relay IN              │
│  GPIO 27 ────► Push Button (to GND)  │
│  GPIO 25 ────► Passive Buzzer (+)    │
│  GPIO 32 ────► I2C SDA (RTC, LCD)    │
│  GPIO 33 ────► I2C SCL (RTC, LCD)    │
│  GPIO 18 ────► SPI SCK (RFID)        │
│  GPIO 19 ────► SPI MISO (RFID)       │
│  GPIO 23 ────► SPI MOSI (RFID)       │
│  5V     ────► Power for modules      │
│  GND    ────► Common ground          │
└─────────────────────────────────────┘
```

### Detailed Connections

#### MFRC522 RFID Reader
| MFRC522 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| SDA (SS) | GPIO 21 | SPI Chip Select |
| SCK | GPIO 18 | SPI Clock |
| MOSI | GPIO 23 | SPI Master Out Slave In |
| MISO | GPIO 19 | SPI Master In Slave Out |
| RST | GPIO 22 | Reset pin |
| 3.3V | 3.3V | Power (3.3V) |
| GND | GND | Ground |

#### Relay Module
| Relay Module | ESP32 Pin | Description |
|--------------|-----------|-------------|
| IN (Signal) | GPIO 26 | Control signal |
| VCC | 5V | Power supply |
| GND | GND | Ground |
| NO/NC/COM | Machine | Connect to your machine/load |

**Note**: This relay module is **ACTIVE LOW** (BC547 transistor). LOW signal = Relay ON, HIGH signal = Relay OFF.

#### 16x2 I2C LCD Display
| LCD I2C Module | ESP32 Pin | Description |
|----------------|-----------|-------------|
| SDA | GPIO 32 | I2C Data line |
| SCL | GPIO 33 | I2C Clock line |
| VCC | 5V | Power supply |
| GND | GND | Ground |

**I2C Address**: Usually 0x27 or 0x3F (check with I2C scanner)

#### DS3231 RTC Module
| DS3231 Pin | ESP32 Pin | Description |
|------------|-----------|-------------|
| SDA | GPIO 32 | I2C Data line (shared with LCD) |
| SCL | GPIO 33 | I2C Clock line (shared with LCD) |
| VCC | 5V | Power supply |
| GND | GND | Ground |

**I2C Address**: 0x68 (fixed)

#### Passive Buzzer
| Buzzer | ESP32 Pin | Description |
|--------|-----------|-------------|
| Positive (+) | GPIO 25 | PWM signal (PWM capable pin) |
| Negative (-) | GND | Ground |

#### Push Button
| Button | ESP32 Pin | Description |
|--------|-----------|-------------|
| One terminal | GPIO 27 | Digital input (with internal pull-up) |
| Other terminal | GND | Ground |

**Note**: Button uses internal pull-up resistor. Pressing button connects GPIO 27 to GND (LOW state).

### Power Connections
- **ESP32**: USB power or 5V adapter
- **All Modules**: Share common GND
- **Relay Module**: Requires 5V (can draw up to 70mA)
- **LCD**: Requires 5V
- **RTC**: Requires 5V (has backup battery)
- **RFID**: Requires 3.3V (ESP32 3.3V pin)
- **Buzzer**: Requires 5V (low current)

## 📐 Circuit Diagram

```
                    ┌─────────────┐
                    │   ESP32     │
                    │   DevKit    │
                    └──────┬──────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
    ┌───▼───┐         ┌───▼───┐         ┌───▼───┐
    │ MFRC  │         │ Relay │         │Button │
    │ 522   │         │Module │         │       │
    │       │         │       │         │  GND  │
    └───────┘         └───┬───┘         └───────┘
                          │
                    ┌─────▼─────┐
                    │  Machine  │
                    │   Load    │
                    └───────────┘

    ┌─────────┐    ┌─────────┐    ┌─────────┐
    │   LCD   │    │  DS3231 │    │ Buzzer  │
    │  I2C    │    │   RTC   │    │         │
    └─────────┘    └─────────┘    └─────────┘
         │              │              │
         └──────┬───────┴──────┬───────┘
                │              │
            I2C Bus        GPIO 25
         (GPIO 32/33)
```

## 💻 Software Requirements

### Arduino IDE Setup

1. **Install Arduino IDE** (version 1.8.19 or later)
   - Download from: https://www.arduino.cc/en/software

2. **Install ESP32 Board Support**
   - Open Arduino IDE → File → Preferences
   - Add to Additional Board Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search "ESP32" and install "esp32 by Espressif Systems"

3. **Install Required Libraries**
   - Open Tools → Manage Libraries
   - Install the following libraries:
     - **MFRC522** by GithubCommunity (version 1.4.10)
     - **LiquidCrystal_I2C** by Frank de Brabander (version 1.1.2)

4. **Select Board and Port**
   - Tools → Board → ESP32 Arduino → ESP32 Dev Module
   - Tools → Port → Select your COM port
   - Tools → Upload Speed → 115200
   - Tools → CPU Frequency → 240MHz (WiFi/BT)
   - Tools → Flash Frequency → 80MHz
   - Tools → Flash Size → 4MB (32Mb)
   - Tools → Partition Scheme → Default 4MB with spiffs

## 📥 Installation

1. **Clone or Download Repository**
   ```bash
   git clone https://github.com/yourusername/rfid-access-control.git
   cd rfid-access-control
   ```

2. **Open Project**
   - Open `rfid_v1_co.ino` in Arduino IDE

3. **Configure Settings** (if needed)
   - Edit pin definitions in code (lines 31-35)
   - Adjust relay configuration (line 105): `RELAY_ACTIVE_HIGH = false`
   - Change LCD I2C address if needed (line 57): `LiquidCrystal_I2C lcd(0x27, 16, 2);`

4. **Upload Code**
   - Click Upload button (→) or press Ctrl+U
   - Wait for "Done uploading" message

5. **Open Serial Monitor**
   - Tools → Serial Monitor
   - Set baud rate to 115200
   - You should see initialization messages

## ⚙️ Configuration

### WiFi Access Point Settings
```cpp
const char* AP_SSID = "MachineAccess";  // WiFi network name
const char* AP_PASS = "12345678";        // WiFi password
```
**Default IP**: 192.168.4.1

### Relay Configuration
```cpp
const bool RELAY_ACTIVE_HIGH = false;  // Set to true if relay is active HIGH
```
- **false** = Active LOW (most relay modules with BC547)
- **true** = Active HIGH (less common)

### LCD I2C Address
```cpp
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to 0x3F if needed
```

### Storage Limits
```cpp
static const size_t MAX_USERS = 500;   // Maximum users in EEPROM
static const size_t MAX_LOGS = 1000;   // Maximum log entries
```

## 🚀 Usage

### Initial Setup

1. **Power On System**
   - Connect ESP32 to power
   - Wait for system initialization (buzzer beeps twice)
   - LCD displays "RFID System Ready"

2. **Connect to WiFi**
   - On your phone/computer, connect to WiFi: **MachineAccess**
   - Password: **12345678**

3. **Access Web Interface**
   - Open browser: http://192.168.4.1
   - Dashboard should load automatically

4. **Set RTC Time**
   - Click "⏰ Set Time" in navigation
   - Enter current date and time
   - Click "Set Time"

5. **Register First User**
   - Click "➕ Add User"
   - Tap an RFID card (UID will auto-fill)
   - Enter Roll/ID and Name
   - Click "Add User"

### Daily Operation

#### Starting a Session
1. Tap registered RFID card on reader
2. System will:
   - Beep (success sound)
   - Turn relay OFF (machine stops)
   - Display "Machine: OFF" on LCD
   - Show user name on LCD
   - Start session timer
   - Log session start time

#### Ending a Session
1. Press the push button
2. System will:
   - Beep (button click + session end)
   - Turn relay ON (machine starts)
   - Display "Machine: ON" on LCD
   - Show session duration
   - Log session end time
   - Save to EEPROM

### Web Interface Features

#### Dashboard
- Real-time system status
- Current session information
- Quick statistics
- Auto-refreshes every second

#### Users Management
- View all registered users
- Search by UID, roll, or name
- Add new users with RFID card
- Edit user information
- Delete users
- Export users as CSV

#### Session Logs
- View all session history
- See start time, end time, duration
- Delete individual logs
- Clear all logs
- Export logs as CSV

#### Statistics
- Total sessions count
- Total usage time
- Average session duration
- Most active user
- EEPROM usage statistics

## 🌐 Web Interface

### Pages Overview

1. **Dashboard** (`/`)
   - System status and live updates
   - Current session details
   - Quick statistics

2. **Users** (`/users`)
   - List of all registered users
   - Search and filter functionality
   - Edit/Delete actions

3. **Add User** (`/add-user`)
   - Register new RFID cards
   - Auto-detects UID when card is tapped
   - Form validation

4. **Logs** (`/logs`)
   - Session history
   - Sortable table
   - Export functionality

5. **Statistics** (`/stats`)
   - Usage analytics
   - Performance metrics
   - System insights

6. **Set Time** (`/time`)
   - Configure DS3231 RTC
   - Manual time setting
   - Current time display

### Mobile Support
- Fully responsive design
- Touch-friendly interface
- Optimized for smartphones
- Works on tablets and desktops

## 🏗️ System Architecture

### Data Flow

```
RFID Card Tap
    ↓
MFRC522 Reader → ESP32
    ↓
User Authentication (EEPROM lookup)
    ↓
Valid User? → YES → Relay Control (OFF)
    ↓                    ↓
    NO              Session Start
    ↓                    ↓
Error Beep         LCD Update
                    Buzzer Feedback
                    Log Creation
                    
Button Press
    ↓
GPIO 27 (LOW)
    ↓
Session End
    ↓
Relay Control (ON)
    ↓
Duration Calculation
    ↓
Log Save (EEPROM)
    ↓
LCD Display (Duration)
```

### Storage Architecture

```
EEPROM (Preferences Library)
├── Users Storage
│   ├── user_count
│   ├── user_uid_0, user_roll_0, user_name_0, user_time_0
│   ├── user_uid_1, user_roll_1, user_name_1, user_time_1
│   └── ... (up to 500 users)
│
└── Logs Storage
    ├── log_count
    ├── log_start_0, log_end_0, log_dur_0, log_uid_0, log_roll_0, log_name_0
    ├── log_start_1, log_end_1, log_dur_1, log_uid_1, log_roll_1, log_name_1
    └── ... (up to 1000 logs, FIFO rotation)
```

### Component Communication

```
┌──────────┐     SPI      ┌──────────┐
│  MFRC522 │◄─────────────►│  ESP32   │
│  RFID    │               │          │
└──────────┘               │          │
                           │          │     I2C
┌──────────┐     I2C      │          │◄─────────┐
│   LCD    │◄─────────────►│          │          │
│  Display │               │          │          │
└──────────┘               │          │     I2C  │
                           │          │◄─────────┤
┌──────────┐     I2C      │          │          │
│  DS3231  │◄─────────────►│          │          │
│   RTC    │               │          │          │
└──────────┘               │          │          │
                           │          │          │
┌──────────┐    Digital   │          │          │
│  Relay   │◄─────────────►│          │          │
│  Module  │   GPIO 26    │          │          │
└──────────┘               │          │          │
                           │          │          │
┌──────────┐    Digital   │          │          │
│  Button  │◄─────────────►│          │          │
│          │   GPIO 27    │          │          │
└──────────┘               │          │          │
                           │          │          │
┌──────────┐     PWM      │          │          │
│  Buzzer  │◄─────────────►│          │          │
│          │   GPIO 25    │          │          │
└──────────┘               └──────────┘          │
                                                │
                                        ┌───────┴───────┐
                                        │   WiFi AP      │
                                        │  (SoftAP)      │
                                        │  192.168.4.1   │
                                        └────────────────┘
```

## 🔍 Troubleshooting

### Relay Not Working

**Problem**: Relay doesn't activate when RFID card is tapped

**Solutions**:
1. Check wiring: Relay IN → GPIO 26, VCC → 5V, GND → GND
2. Verify relay module has power (LED should be on)
3. Try changing `RELAY_ACTIVE_HIGH` in code:
   ```cpp
   const bool RELAY_ACTIVE_HIGH = true;  // Change from false to true
   ```
4. Check Serial Monitor for relay activation messages
5. Test relay manually via web interface (if test option available)
6. Verify relay module specifications match (5V, active LOW)

### RFID Not Reading Cards

**Problem**: No response when tapping RFID cards

**Solutions**:
1. Check SPI connections (SCK, MISO, MOSI, SS, RST)
2. Verify MFRC522 is powered with 3.3V (not 5V)
3. Ensure RFID cards are 13.56MHz (Mifare compatible)
4. Check Serial Monitor for "RFID init OK" message
5. Try different RFID cards
6. Verify MFRC522 module is not damaged

### LCD Not Displaying

**Problem**: LCD screen is blank or shows garbage

**Solutions**:
1. Check I2C connections (SDA → GPIO 32, SCL → GPIO 33)
2. Verify I2C address (try 0x27 or 0x3F)
3. Adjust LCD contrast potentiometer (if available)
4. Check power supply (5V)
5. Verify I2C pull-up resistors (usually built into module)
6. Use I2C scanner to find correct address

### RTC Time Not Accurate

**Problem**: RTC shows wrong time or loses time

**Solutions**:
1. Set RTC time via web interface
2. Check RTC module has backup battery
3. Verify I2C connections
4. Replace RTC battery if old
5. Check RTC module is powered (5V)

### WiFi Not Connecting

**Problem**: Cannot connect to "MachineAccess" network

**Solutions**:
1. Check Serial Monitor for "SoftAP start: OK"
2. Verify SSID: "MachineAccess" and password: "12345678"
3. Try forgetting network and reconnecting
4. Check ESP32 WiFi antenna is connected
5. Move closer to ESP32
6. Restart ESP32

### Web Interface Not Loading

**Problem**: Cannot access http://192.168.4.1

**Solutions**:
1. Verify you're connected to "MachineAccess" WiFi
2. Check Serial Monitor for "Server started" message
3. Try different browser
4. Clear browser cache
5. Try IP address: 192.168.4.1
6. Check firewall settings

### Buzzer Not Working

**Problem**: No sound from buzzer

**Solutions**:
1. Verify buzzer is passive (not active)
2. Check connections: + → GPIO 25, - → GND
3. Test with different frequencies
4. Verify GPIO 25 is PWM capable
5. Check buzzer is not damaged

### EEPROM Full

**Problem**: Cannot add more users or logs

**Solutions**:
1. Delete unused users
2. Clear old logs
3. Increase limits in code (if ESP32 has more flash)
4. Export data before clearing
5. Consider using external storage for large datasets

## 📁 Project Structure

```
rfid-access-control/
│
├── rfid_v1_co.ino          # Main Arduino sketch
├── README.md               # This file
├── LICENSE                 # License file
│
├── docs/                   # Documentation (optional)
│   ├── wiring-diagram.png
│   ├── pinout-diagram.png
│   └── flowcharts/
│
└── examples/               # Example configurations (optional)
    ├── custom-pins.ino
    └── alternative-config.h
```

## 🎯 Key Features Explained

### Session Management
- **Start**: RFID card tap → Relay OFF → Session begins
- **End**: Button press → Relay ON → Session ends → Duration logged
- **Tracking**: All sessions logged with timestamps and duration

### Data Persistence
- **EEPROM Storage**: All data stored in ESP32 flash memory
- **No SD Card**: Eliminates SD card dependency
- **Automatic Backup**: Data persists across power cycles
- **FIFO Rotation**: Old logs automatically removed when limit reached

### Real-time Updates
- **Auto-refresh**: Dashboard updates every second
- **Live Status**: See current session in real-time
- **Instant Feedback**: Buzzer and LCD provide immediate feedback

### Security Features
- **WiFi Password**: Protected access point
- **User Authentication**: Only registered cards work
- **Session Logging**: Complete audit trail

## 🔬 Technical Specifications

### ESP32 Configuration
- **CPU**: Dual-core 240MHz
- **Flash**: 4MB
- **RAM**: 520KB
- **WiFi**: 802.11 b/g/n
- **GPIO**: 34 digital pins

### Performance
- **RFID Read Time**: < 100ms
- **Web Response**: < 50ms
- **EEPROM Write**: < 10ms per entry
- **Max Users**: 500
- **Max Logs**: 1000

### Power Consumption
- **Idle**: ~80mA @ 5V
- **Active (WiFi)**: ~120mA @ 5V
- **Relay ON**: +70mA (relay module)
- **Total Max**: ~200mA @ 5V

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👨‍💻 Author

**Your Name**
- GitHub: [@yourusername](https://github.com/yourusername)
- Email: your.email@example.com

## 🙏 Acknowledgments

- ESP32 community for excellent documentation
- MFRC522 library developers
- LiquidCrystal_I2C library developers
- All contributors and testers

## 📞 Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check existing issues for solutions
- Review troubleshooting section above

## 🗺️ Roadmap

Future enhancements:
- [ ] MQTT integration for remote monitoring
- [ ] Email notifications for events
- [ ] Multi-machine support
- [ ] Advanced reporting and analytics
- [ ] Mobile app development
- [ ] Cloud backup integration
- [ ] Admin authentication for web interface
- [ ] Scheduled maintenance reminders

---

**⭐ If you find this project useful, please give it a star!**
