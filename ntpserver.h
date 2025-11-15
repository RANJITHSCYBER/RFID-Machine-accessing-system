// ntpserver.h
// Lightweight NTP + RTC sync library for ESP8266/ESP32/Arduino with WiFi
// Provides: connectWiFiAndSync(), getDateTime(), begin()

#ifndef NTPSERVER_H
#define NTPSERVER_H

#include <Arduino.h>

#if defined(ESP8266) || defined(ESP32)
#include <WiFi.h>
#else
#error "This library requires ESP8266 or ESP32"
#endif

#include <WiFiUdp.h>

// Optional RTC support - user can provide a callback to set/get RTC
// or link with a hardware RTC library (e.g., RTClib) in the sketch.

class NTPServerLib {
public:
  NTPServerLib();

  // begin with optional custom NTP server and timezone offset in seconds
  void begin(const char* ntpServer = "pool.ntp.org", long gmtOffsetSec = 19800, int retryCount = 5);

  // connect to WiFi (blocking) and sync time using NTP. If RTC callbacks provided,
  // RTC will be updated. Returns true if time successfully obtained from NTP or RTC.
  bool connectWiFiAndSync(const char* ssid, const char* password, unsigned long timeoutMs = 20000);

  // Provide RTC setter callback: void setRTC(struct tm*)
  void setRTCWriter(void (*writer)(const struct tm* t));
  // Provide RTC reader callback: bool readRTC(struct tm* out) - returns true if RTC had valid time
  void setRTCReader(bool (*reader)(struct tm* out));

  // Get last known time (after sync). Returns true if available
  bool getDateTime(struct tm* out);

private:
  const char* _ntpServer;
  long _gmtOffsetSec;
  int _retryCount;
  WiFiUDP _udp;
  unsigned long _lastSync = 0;
  time_t _lastEpoch = 0;
  void (*_rtcWriter)(const struct tm* t) = nullptr;
  bool (*_rtcReader)(struct tm* out) = nullptr;

  bool syncFromNTP();
  bool syncFromRTC();
};

#endif
