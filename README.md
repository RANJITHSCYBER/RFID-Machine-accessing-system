# NTPServer library (ntpserver)

Lightweight helper to sync time from NTP (with IST offset by default) and optionally keep a hardware RTC in sync.

Files added/installed:
- `libraries/ntpserver/ntpserver.h` - library header
- `libraries/ntpserver/ntpserver.cpp` - implementation
Note: The example sketch was removed to avoid conflicts with the main project. If you want the example restored, it's available on request.

Notes:
- Target: ESP8266 or ESP32
- By default uses `pool.ntp.org` and IST offset (19800 seconds). Change via `begin()`.
- Optionally provide RTC read/write callbacks if using a hardware RTC (e.g. DS1307 via RTClib).
- If NTP fails, the library will fall back to RTC if provided.

How to use:
1. Place files in your Arduino sketch directory or create a library folder.
2. Edit `ntp_example.ino` with your WiFi credentials.
3. If using an RTC, enable the RTClib portions and implement the callbacks.
4. Upload to your ESP board and open Serial at 115200.
