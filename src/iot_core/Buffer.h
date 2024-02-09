#ifndef IOT_CORE_BUFFER_H_
#define IOT_CORE_BUFFER_H_

#include <toolbox.h>

namespace iot_core {

/**
 * Class template for a plain char buffer, which can be used to create e.g.
 * messages to be sent via serial or network.
 */
template<size_t BUFFER_SIZE = 512u>
class Buffer final {
  char _buffer[BUFFER_SIZE] = {};
  size_t _size = 0u;
  bool _overrun = false;

public:
  Buffer() {}

  const char* c_str() const {
    return _buffer;
  }

  const uint8_t* data() const {
    return (uint8_t*)(&_buffer[0]);
  }

  size_t size() const {
    return _size;
  }

  bool overrun() const {
    return _overrun;
  }

  void clear() {
    _size = 0u;
    _overrun = false;
  }

  size_t write(toolbox::strref& data) {
    if (_overrun) {
      return 0;
    }

    size_t copiedLength = data.copy(_buffer + _size, BUFFER_SIZE - _size, false);
    _size += copiedLength;

    if (copiedLength < data.length()) {
      _overrun = true;
    }

    return copiedLength;
  }

  size_t write(char c) {
    if (_size >= BUFFER_SIZE) {
      _overrun = true;
      return 0u;
    }
    _buffer[_size++] = c;
    return 1u;
  }
};

}

#endif
