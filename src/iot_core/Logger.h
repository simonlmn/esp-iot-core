#ifndef IOT_CORE_LOGGER_H_
#define IOT_CORE_LOGGER_H_

#include "Utils.h"
#include "DateTime.h"
#include <toolbox.h>
#include <functional>
#include <type_traits>
#include <algorithm>
#include <vector>

namespace iot_core {

static const size_t MAX_LOG_ENTRY_LENGTH = 128u;
static const char LOG_ENTRY_SEPARATOR = '\n';

struct LogEntry {
  char buffer[MAX_LOG_ENTRY_LENGTH + 1] = {};
  size_t length = 0u;
};
static LogEntry g_logEntry; // Globally shared log entry buffer for building/processing single log entries

enum struct LogLevel : uint8_t {
  None = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
  Unknown = 254,
  All = 255,
};

toolbox::strref logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::None: return F("---");
    case LogLevel::Error: return F("ERR");
    case LogLevel::Warning: return F("WRN");
    case LogLevel::Info: return F("INF");
    case LogLevel::Debug: return F("DBG");
    case LogLevel::Trace: return F("TRC");
    case LogLevel::All: return F("ALL");
    default: return F("???");
  }
}

LogLevel logLevelFromString(const toolbox::strref& level) {
  if (level == F("---")) return LogLevel::None;
  if (level == F("ERR")) return LogLevel::Error;
  if (level == F("WRN")) return LogLevel::Warning;
  if (level == F("INF")) return LogLevel::Info;
  if (level == F("DBG")) return LogLevel::Debug;
  if (level == F("TRC")) return LogLevel::Trace;
  if (level == F("ALL")) return LogLevel::All;
  return LogLevel::Unknown;
}

class LogService;

class Logger final {
  mutable LogService* _service;
  const char* _category;

public:
  Logger(LogService& service, const char* category) : _service(&service), _category(category) {}

  template<typename T>
  void log(T message) const;

  template<typename T, std::enable_if_t<!std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, T message) const;

  template<typename T, std::enable_if_t<std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, T messageFunction) const;
};

class ILogSink {
public:
  virtual void enable(bool enabled) = 0;  
  virtual bool enabled() const = 0;
  virtual void logLevel(LogLevel level) = 0;
  virtual LogLevel logLevel() const = 0;
  virtual void commitLogEntry(const char* entry) = 0;
};

class ILocalLogSink : public ILogSink {
public:
  virtual void output(std::function<void(const char* entry)> handler) const = 0;
};

class LogService final {
  static const LogLevel DEFAULT_LOG_LEVEL = LogLevel::Info;
  LogLevel _initialLogLevel = DEFAULT_LOG_LEVEL;
  ConstStrMap<LogLevel> _logLevels = {};
  Time const& _uptime;
  std::vector<ILogSink*> _sinks;

  template<typename T>
  void logInternal(LogLevel level, const char* category, T message) {
    beginLogEntry(level, category);
    g_logEntry.length += toolbox::strref(message).copy(g_logEntry.buffer + g_logEntry.length, MAX_LOG_ENTRY_LENGTH - g_logEntry.length, true);
    commitLogEntry(level);
  }

  void beginLogEntry(LogLevel level, const char* category) {
    size_t actualLength = snprintf_P(g_logEntry.buffer, MAX_LOG_ENTRY_LENGTH, PSTR("[%s|%s|%s] "), _uptime.format(), category, logLevelToString(level).cstr());
    g_logEntry.length = std::min(actualLength, MAX_LOG_ENTRY_LENGTH);
  }

  void commitLogEntry(LogLevel level) {
    if (g_logEntry.length == 0u) {
      return;
    }
    g_logEntry.buffer[g_logEntry.length] = LOG_ENTRY_SEPARATOR;
    g_logEntry.buffer[g_logEntry.length + 1u] = '\0';

    for (auto sink : _sinks) {
      if (sink->enabled() && sink->logLevel() >= level) {
        sink->commitLogEntry(g_logEntry.buffer);
      }
    }

    g_logEntry.length = 0u;
    g_logEntry.buffer[g_logEntry.length] = '\0';
  }

public:
  explicit LogService(Time const& uptime) : _uptime(uptime), _sinks() {}

  Logger logger(const char* category) {
    return {*this, category};
  }

  LogLevel initialLogLevel() const {
    return _initialLogLevel;
  }

  void initialLogLevel(LogLevel level) {
    _initialLogLevel = level;
  }

  LogLevel logLevel(const char* category) const {
    auto entry = _logLevels.find(category);
    if (entry == _logLevels.end()) {
      return _initialLogLevel;
    } else {
      return entry->second;
    }
  }

  const ConstStrMap<LogLevel>& logLevels() const {
    return _logLevels;
  }

  void logLevel(const char* category, LogLevel level) {
    _logLevels[category] = level;
  }

  void clearLogLevel(const char* category) {
    _logLevels.erase(category);
  }

  template<typename T>
  void log(const char* category, T message) {
    logInternal(LogLevel::None, category, message);
  }

  template<typename T, std::enable_if_t<!std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, const char* category, T message) {
    if (level <= logLevel(category)) {
      logInternal(level, category, message);
    }
  };

  template<typename T, std::enable_if_t<std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, const char* category, T messageFunction) {
    if (level <= logLevel(category)) {
      logInternal(level, category, messageFunction());
    }
  };

  void addLogSink(ILogSink& sink) {
    _sinks.push_back(&sink);
  }

  void removeLogSink(ILogSink& sink) {
    _sinks.erase(std::remove(_sinks.begin(), _sinks.end(), &sink), _sinks.end());
  }

  const std::vector<ILogSink*>& logSinks() const {
    return _sinks;
  }
};

template<typename T>
void Logger::log(T message) const {
  _service->log(_category, message);
}

template<typename T, std::enable_if_t<!std::is_invocable<T>::value, bool> = true>
void Logger::log(LogLevel level, T message) const {
  _service->log(level, _category, message);
};

template<typename T, std::enable_if_t<std::is_invocable<T>::value, bool> = true>
void Logger::log(LogLevel level, T messageFunction) const {
  _service->log(level, _category, messageFunction);
};

}

#endif
