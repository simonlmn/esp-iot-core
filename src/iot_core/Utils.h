#ifndef IOT_CORE_UTILS_H_
#define IOT_CORE_UTILS_H_

#include <toolbox.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <type_traits>

namespace iot_core {

class IntervalTimer final {
  unsigned long _intervalDurationMs;
  unsigned long _lastIntervalTimeMs;
public:
  IntervalTimer(unsigned long intervalDurationMs) : _intervalDurationMs(intervalDurationMs), _lastIntervalTimeMs(millis()) {}

  bool elapsed() const {
    return (_lastIntervalTimeMs + _intervalDurationMs) < millis(); 
  }

  void restart() { _lastIntervalTimeMs = millis(); }
};


template<size_t SAMPLES>
class TimingStatistics final {
  unsigned long _samples[SAMPLES] = {};
  bool _hasSamples = false;
  size_t _oldestSampleIndex = 0u;
  size_t _newestSampleIndex = 0u;

  unsigned long _startTime = 0u;

  void newSample(unsigned long value) {
    if (_hasSamples) {
      _newestSampleIndex = (_newestSampleIndex + 1) % SAMPLES;
      if (_newestSampleIndex == _oldestSampleIndex) {
        _oldestSampleIndex = (_oldestSampleIndex + 1) % SAMPLES;
      }
    }

    _samples[_newestSampleIndex] = value;

    _hasSamples = true;
  }
  
public:
  void start() {
    _startTime = micros();
  }

  void stop() {
    newSample(micros() - _startTime);
  }

  unsigned long min() const {
    size_t numberOfSamples = count();
    unsigned long result = _samples[_oldestSampleIndex];
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result = std::min(_samples[(_oldestSampleIndex + i) % SAMPLES], result);
    }
    return result;
  }

  unsigned long max() const {
    size_t numberOfSamples = count();
    unsigned long result = _samples[_oldestSampleIndex];
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result = std::max(_samples[(_oldestSampleIndex + i) % SAMPLES], result);
    }
    return result;
  }

  unsigned long avg() const {
    size_t numberOfSamples = count();
    if (numberOfSamples == 0u) {
      return 0u;
    }

    unsigned long result = _samples[_oldestSampleIndex] / numberOfSamples;
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result += _samples[(_oldestSampleIndex + i) % SAMPLES] / numberOfSamples;
    }
    return result;
  }

  size_t count() const { return _hasSamples ? (_newestSampleIndex >= _oldestSampleIndex ? (_newestSampleIndex - _oldestSampleIndex + 1) : (SAMPLES - _oldestSampleIndex + _newestSampleIndex + 1)) : 0u; }

  template<typename F>
  auto wrap(F f) {
    return [this,f] () {
      this->start();
      f();
      this->stop();
    };
  }
};

struct str_less_than
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

template<typename T>
using ConstStrMap = std::map<const char*, T, str_less_than>;

using ConstStrSet = std::set<const char*, str_less_than>;

}

#endif
