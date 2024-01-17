#ifndef IOT_CORE_API_CHUNKEDRESPONSE_H_
#define IOT_CORE_API_CHUNKEDRESPONSE_H_

#include <iot_core/Utils.h>

namespace iot_core::api {

/**
 * Class template to send a chunked HTTP response to a client.
 * 
 * The buffer size influences how many chunks have to be sent and each chunk
 * incurs a (small) processing and transmission overhead. Thus setting the
 * size too low may reduce performance.
 */
template<typename T, size_t BUFFER_SIZE = 512u>
class ChunkedResponse final {
  T& _server;
  char _buffer[BUFFER_SIZE + 1u] = {}; // +1 for null-termination
  size_t _size = 0u;
  bool _valid = false;

public:
  explicit ChunkedResponse(T& server) : _server(server) {}

  size_t size() const {
    return _size;
  }

  bool valid() const {
    return _valid;
  }

  void clear() {
    _size = 0u;
  }

  template<typename TText>
  bool begin(int code, const TText* contentType) {
    clear();
    _valid = _server.chunkedResponseModeStart(code, contentType);
    return _valid;
  }

  void flush() {
    if (!_valid || _size == 0u) {
      return;
    }

    _server.sendContent(_buffer, _size);
    clear();
  }

  void end() {
    if (!_valid) {
      return;
    }

    flush();
    _server.chunkedResponseFinalize();
    _valid = false;
  }

  template<typename X>
  size_t write(X text) {
    return write(iot_core::str(text));
  }

  template<typename X>
  size_t write(X data, size_t length) {
    return write(iot_core::data(data, length));
  }

  template<typename U>
  size_t write(iot_core::ConstString<U> string) {
    if (!_valid) {
      return 0u;
    }

    size_t offset = 0u;
    do {
      size_t maxLength = BUFFER_SIZE - _size;
      size_t stringLength = string.copy(_buffer + _size, maxLength, offset);
      
      if (maxLength < stringLength) {
        _size += maxLength;
        offset += maxLength;
        flush();
      } else {
        _size += stringLength;
        break;
      }
    } while (true);
    return string.len();
  }

  size_t write(char c) {
    if (!_valid) {
      return 0u;
    }
    
    if (_size >= BUFFER_SIZE) {
      flush();
    }
    _buffer[_size] = c;
    _size += 1u;
    return 1u;
  }
};

}

#endif
