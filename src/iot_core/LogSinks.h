#ifndef IOT_CORE_LOGSINKS_H_
#define IOT_CORE_LOGSINKS_H_

#include "Logger.h"
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

namespace iot_core {

class InMemoryLogSink final : public ILocalLogSink {
  static const size_t LOG_BUFFER_SIZE = 4096u;

  bool _enabled = true;
  LogLevel _logLevel = LogLevel::Info;

  char _logBuffer[LOG_BUFFER_SIZE] = {};
  size_t _logBufferStart = 0u;
  size_t _logBufferEnd = 0u;

  void advanceBufferStart() {
    while (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
    }
    _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
  }

public:
  InMemoryLogSink() {
    memset(_logBuffer, LOG_ENTRY_SEPARATOR, LOG_BUFFER_SIZE);
  }

  void enable(bool enabled) override {
    _enabled = enabled;
  }

  bool enabled() const override {
    return _enabled;
  }

  void logLevel(LogLevel level) override {
    _logLevel = level;
  }

  LogLevel logLevel() const override {
    return _logLevel;
  }

  void commitLogEntry(const char* entry) override {
    if (!enabled()) {
      return;
    }

    for (unsigned short i = 0u; entry[i] != '\0'; ++i) {
      if (_logBufferEnd == LOG_BUFFER_SIZE) {
        _logBufferEnd = 0u;
        if (_logBufferStart == 0u) {
          advanceBufferStart();
        }
      } else if (_logBufferStart == _logBufferEnd && _logBufferStart != 0u) {
        advanceBufferStart();
      }

      _logBuffer[_logBufferEnd] = entry[i];
      _logBufferEnd += 1u;
    }
  }

  void output(std::function<void(const char* entry)> handler) const override {
    if (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      unsigned short bufferIndex = _logBufferStart;
      unsigned short entryIndex = 0u;
      do {
        bufferIndex = bufferIndex % LOG_BUFFER_SIZE;
        g_logEntry.buffer[entryIndex] = _logBuffer[bufferIndex];

        if (g_logEntry.buffer[entryIndex] == LOG_ENTRY_SEPARATOR) {
          g_logEntry.buffer[entryIndex + 1u] = '\0';
          handler(g_logEntry.buffer);
          entryIndex = 0u;
        } else {
          entryIndex += 1u;
        }

        bufferIndex = bufferIndex + 1u;
      } while (bufferIndex != _logBufferEnd);
    }
  }
};

class UdpLogSink final : public ILogSink {
  bool _enabled = false;
  LogLevel _logLevel = LogLevel::All;

  WiFiUDP _socket;
  IPAddress _remoteAddress;
  uint16_t _remotePort;

public:
  UdpLogSink() :
    _remoteAddress(127, 0, 0, 1),
    _remotePort(5141)
  {}

  void destination(IPAddress address, uint16_t port) {
    _remoteAddress = address;
    _remotePort = port;
  }

  void enable(bool enabled) override {
    _enabled = enabled;
  }

  bool enabled() const override {
    return _enabled;
  }

  void logLevel(LogLevel level) override {
    _logLevel = level;
  }

  LogLevel logLevel() const override {
    return _logLevel;
  }

  void commitLogEntry(const char* entry) override {
    if (!enabled()) {
      return;
    }

    if (WiFi.status() != WL_CONNECTED) {
      return;
    }

    if (_socket.beginPacket(_remoteAddress, _remotePort) == 1) {
      _socket.write((const uint8_t*)entry, strlen(entry));
      _socket.endPacket();
    }
  }
};

}

#endif // IOT_CORE_LOGSINKS_H_