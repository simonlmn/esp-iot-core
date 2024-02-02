#ifndef IOT_CORE_API_CHUNKEDRESPONSE_H_
#define IOT_CORE_API_CHUNKEDRESPONSE_H_

#include <toolbox.h>
#include <toolbox/Streams.h>

namespace iot_core::api {

/**
 * Class template to send a chunked HTTP response to a client.
 * 
 * The buffer size influences how many chunks have to be sent and each chunk
 * incurs a (small) processing and transmission overhead. Thus setting the
 * size too low may reduce performance.
 */
template<typename T, size_t BUFFER_SIZE = 512u>
class ChunkedResponse final : public toolbox::IOutput {
  T& _server;
  char _buffer[BUFFER_SIZE + 1u] = {}; // +1 for null-termination
  size_t _size = 0u;
  bool _valid = false;

public:
  explicit ChunkedResponse(T& server) : _server(server) {}

  ~ChunkedResponse() { end(); }

  size_t size() const {
    return _size;
  }

  bool valid() const {
    return _valid;
  }

  void clear() {
    _size = 0u;
  }

  bool begin(int code, const toolbox::strref& contentType) {
    clear();
    if (contentType.isInProgmem()) {
      _valid = _server.chunkedResponseModeStart(code, contentType.fpstr());
    } else {
      _valid = _server.chunkedResponseModeStart(code, contentType.cstr());
    }
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

  size_t write(const toolbox::strref& string) {
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

  size_t write(char c) override {
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

  size_t write(const char* string) override {
    return write(toolbox::strref{string});
  }

  size_t write(const __FlashStringHelper* string) override {
    return write(toolbox::strref{string});
  }
};

}

#endif
