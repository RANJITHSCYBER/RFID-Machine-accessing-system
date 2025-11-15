// ntpserver.cpp
#include "ntpserver.h"
#include <time.h>

#define NTP_PACKET_SIZE 48

static const uint8_t ntpPacketBufferDefault[NTP_PACKET_SIZE] = {0};

NTPServerLib::NTPServerLib() {
}

void NTPServerLib::begin(const char* ntpServer, long gmtOffsetSec, int retryCount) {
  _ntpServer = ntpServer;
  _gmtOffsetSec = gmtOffsetSec;
  _retryCount = retryCount;
}

void NTPServerLib::setRTCWriter(void (*writer)(const struct tm* t)) {
  _rtcWriter = writer;
}

void NTPServerLib::setRTCReader(bool (*reader)(struct tm* out)) {
  _rtcReader = reader;
}

bool NTPServerLib::getDateTime(struct tm* out) {
  if (_lastEpoch == 0) {
    // try RTC
    return syncFromRTC() && _rtcReader && _rtcReader(out);
  }
  time_t now = _lastEpoch + (time(nullptr) - _lastSync);
  gmtime_r(&now, out);
  return true;
}

bool NTPServerLib::connectWiFiAndSync(const char* ssid, const char* password, unsigned long timeoutMs) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      delay(200);
    }
  }

  // First try NTP sync
  for (int i = 0; i < _retryCount; ++i) {
    if (syncFromNTP()) return true;
    delay(500);
  }

  // If NTP fails, try RTC reader if available
  if (_rtcReader) {
    struct tm tm;
    if (_rtcReader(&tm)) {
      // set internal epoch from RTC
      _lastEpoch = mktime(&tm);
      _lastSync = time(nullptr);
      return true;
    }
  }

  return false;
}

bool NTPServerLib::syncFromNTP() {
  if (_ntpServer == nullptr) _ntpServer = "pool.ntp.org";

  IPAddress timeServerIP;
  if (!WiFi.hostByName(_ntpServer, timeServerIP)) return false;

  _udp.begin(2390);

  // send packet
  uint8_t packetBuffer[NTP_PACKET_SIZE];
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision

  _udp.beginPacket(timeServerIP, 123);
  _udp.write(packetBuffer, NTP_PACKET_SIZE);
  _udp.endPacket();

  unsigned long start = millis();
  while (millis() - start < 1500) {
    int size = _udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      _udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      epoch += _gmtOffsetSec;
      _lastEpoch = epoch;
      _lastSync = time(nullptr);

      // update system time (for ESPs)
      timeval tv; tv.tv_sec = epoch; tv.tv_usec = 0;
      settimeofday(&tv, NULL);

      // update RTC if writer available
      if (_rtcWriter) {
        struct tm tm;
        gmtime_r((time_t*)&epoch, &tm);
        _rtcWriter(&tm);
      }

      return true;
    }
    delay(10);
  }
  return false;
}

bool NTPServerLib::syncFromRTC() {
  if (!_rtcReader) return false;
  struct tm tm;
  if (!_rtcReader(&tm)) return false;
  _lastEpoch = mktime(&tm);
  _lastSync = time(nullptr);
  timeval tv; tv.tv_sec = _lastEpoch; tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  return true;
}
