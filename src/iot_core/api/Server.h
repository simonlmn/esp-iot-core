#ifndef IOT_CORE_API_SERVER_H_
#define IOT_CORE_API_SERVER_H_

#include <iot_core/Interfaces.h>
#include <iot_core/Utils.h>
#include <ESP8266WebServer.h>
#include <uri/UriGlob.h>
#include <vector>
#include "ChunkedResponse.h"
#include "Interfaces.h"

namespace iot_core::api {


static const char HEADER_ACCEPT[] PROGMEM = "Accept";
static const char HEADER_CONTENT_TYPE[] PROGMEM = "Content-Type";

HTTPMethod mapHttpMethod(HttpMethod method) {
  switch (method) {
    case HttpMethod::ANY: return HTTP_ANY;
    case HttpMethod::DELETE: return HTTP_DELETE;
    case HttpMethod::GET: return HTTP_GET;
    case HttpMethod::HEAD: return HTTP_HEAD;
    case HttpMethod::OPTIONS: return HTTP_OPTIONS;
    case HttpMethod::PATCH: return HTTP_PATCH;
    case HttpMethod::POST: return HTTP_POST;
    case HttpMethod::PUT: return HTTP_PUT;
    default: return HTTP_ANY;
  }
}

int mapResponseCode(ResponseCode code) {
  return static_cast<int>(code);
}

const char* mapContentType(ContentType contentType) {
  switch (contentType) {
    default:
    case ContentType::TextPlain: return "text/plain";
    case ContentType::TextCsv: return "text/csv";
    case ContentType::TextHtml: return "text/html";
    case ContentType::ApplicationOctetStream: return "application/octet-stream";
    case ContentType::ApplicationJson: return "application/json";
    case ContentType::ApplicationXml: return "application/xml";
  }
}

class RequestBody final : public IRequestBody {
private:
  ESP8266WebServer& _server;
  const String& _plainArg;

public:
  explicit RequestBody(ESP8266WebServer& server) : _server(server), _plainArg(_server.arg("plain")) {}

  const char* content() const {
    return _plainArg.c_str();
  }

  size_t length() const override {
    return _plainArg.length();
  }
};

class SingleResponseBody final : public IResponseBody {
private:
  ESP8266WebServer& _server;
  int _responseCode;
  const char* _contentType;
  bool _valid;

public:
  explicit SingleResponseBody(ESP8266WebServer& server) : _server(server), _responseCode(200), _contentType("text/plain"), _valid(false) {}

  void begin(int code, const char* contentType) {
    _responseCode = code;
    _contentType = contentType;
    _valid = true;
  }

  void end() {
    _valid = false;
  }

  bool valid() const override { return _valid; };

  size_t write(const char* text) override {
    if (!_valid) {
      return 0u;
    }
    _server.send(_responseCode, _contentType, text);
    return iot_core::str(text).len();
  }

  size_t write(const char* data, size_t length) override {
    if (!_valid) {
      return 0u;
    }
    _server.send(_responseCode, _contentType, data, length);
    return length;
  }

  size_t write(const __FlashStringHelper* text) override {
    if (!_valid) {
      return 0u;
    }
    _server.send_P(_responseCode, _contentType, (PGM_P)text);
    return iot_core::str(text).len();
  }

  size_t write(const __FlashStringHelper* data, size_t length) override {
    if (!_valid) {
      return 0u;
    }
    _server.send_P(_responseCode, _contentType, (PGM_P)data, length);
    return length;
  }

  size_t write(char c) override {
    if (!_valid) {
      return 0u;
    }
    _server.send(_responseCode, _contentType, &c, 1);
    return 1u;
  }
};

class ChunkedResponseBody final : public IResponseBody { // maybe ChunkedResponse<> could directly implement this interface?
private:
  ChunkedResponse<ESP8266WebServer> _response;

public:
  explicit ChunkedResponseBody(ESP8266WebServer& server) : _response(server) {}
  void begin(int code, const char* contentType) { _response.begin(code, contentType); }
  void end() { _response.end(); }
  bool valid() const override { return _response.valid(); };
  size_t write(const char* text) override { return _response.write(text); }
  size_t write(const char* data, size_t length) override { return _response.write(data, length); }
  size_t write(const __FlashStringHelper* text) override { return _response.write(text); }
  size_t write(const __FlashStringHelper* data, size_t length) override { return _response.write(data, length); }
  size_t write(char c) override { return _response.write(c); }
};

class Request final : public IRequest {
private:
  ESP8266WebServer& _server;
  RequestBody _body;

public:
  explicit Request(ESP8266WebServer& server) : _server(server), _body(server) {}

  bool hasArg(const char* name) const override {
    return _server.hasArg(name);
  }

  const char* arg(const char* name) const override {
    return _server.arg(name).c_str();
  }

  const char* pathArg(unsigned int i) const override {
    return _server.pathArg(i).c_str();
  }

  const IRequestBody& body() const override {
    return _body;
  }
};

class Response final : public IResponse {
private:
  ESP8266WebServer& _server;
  SingleResponseBody _singleBody;
  ChunkedResponseBody _chunkedBody;
  int _code;
  const char* _contentType;
  
public:
  explicit Response(ESP8266WebServer& server) : _server(server), _singleBody(server), _chunkedBody(server), _code(mapResponseCode(ResponseCode::NotImplemented)), _contentType(mapContentType(ContentType::TextPlain)) {}

  virtual ~Response() {
    if (_singleBody.valid()) {
      _singleBody.end();
    } else if (_chunkedBody.valid()) {
      _chunkedBody.end();
    } else {
      _server.send_P(_code, _contentType, "");
    }
  }
  
  IResponse& code(ResponseCode code) override {
    _code = mapResponseCode(code);
    return *this;
  }

  IResponse& contentType(ContentType contentType) override {
    _contentType = mapContentType(contentType);
    return *this;
  }

  IResponse& contentType(const char* contentType) override {
    _contentType = contentType;
    return *this;
  }

  IResponse& header(const char* name, const char* value) override {
    _server.sendHeader(name, value);
    return *this;
  }
  
  IResponseBody& sendChunkedBody() override {
    _chunkedBody.begin(_code, _contentType);
    if (!_chunkedBody.valid()) {
      code(ResponseCode::HttpVersionNotSupported);
      contentType(ContentType::TextPlain);
      sendSingleBody().write(F("HTTP1.1 required"));
    }
    return _chunkedBody;
  }
  
  IResponseBody& sendSingleBody() override {
    _singleBody.begin(_code, _contentType);
    if (!_singleBody.valid()) {
      code(ResponseCode::InternalServerError);
      contentType(ContentType::TextPlain);
    }
    return _singleBody;
  }
};

class Server final : public IServer, public IContainer, public IApplicationComponent {
private:
  Logger& _logger;
  ISystem& _system;
  std::vector<IProvider*> _providers;
  ESP8266WebServer _server;
  TimingStatistics<10u> _callStatistics;
  
public:
  Server(ISystem& system, int port = 80) : _logger(system.logger()), _system(system), _providers(), _server(port) {}
  
  void on(const Uri& uri, HttpMethod method, std::function<void(const IRequest&, IResponse&)> handler) override {
    _server.on(uri, mapHttpMethod(method), _callStatistics.wrap([this,handler]() {
      Request request {_server};
      Response response {_server};
      handler(request, response);
    }));
  }

  void addProvider(IProvider* provider) override {
    _providers.emplace_back(provider);
  }

  const char* name() const override {
    return "api";
  }

  bool configure(const char* /*name*/, const char* /*value*/) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> /*writer*/) const override {
  }

  void setup(bool /*connected*/) override {
    _server.enableCORS(true);
    _server.collectHeaders(FPSTR(HEADER_ACCEPT));

    // generic OPTIONS reply to make "pre-flight" checks work
    on(UriGlob(F("*")), HttpMethod::OPTIONS, [](const IRequest&, IResponse& response) {  
      response.code(ResponseCode::OkNoContent).header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    });
    
    for (auto provider : _providers) {
      provider->setupApi(*this);
    }
  }

  void loop(ConnectionStatus status) override {
    switch (status) {
      case ConnectionStatus::Reconnected:
        _server.begin();
        break;
      case ConnectionStatus::Connected:
        _server.handleClient();
        break;
      case ConnectionStatus::Disconnecting:
        _server.close();
        break;
      case ConnectionStatus::Disconnected:
        // do nothing
        break;
    }
  }

  void getDiagnostics(IDiagnosticsCollector& collector) const override {
    collector.addValue("callCount", iot_core::convert<size_t>::toString(_callStatistics.count(), 10));
    collector.addValue("callAvg", iot_core::convert<unsigned long>::toString(_callStatistics.avg(), 10));
    collector.addValue("callMin", iot_core::convert<unsigned long>::toString(_callStatistics.min(), 10));
    collector.addValue("callMax", iot_core::convert<unsigned long>::toString(_callStatistics.max(), 10));
  }
};

}

#endif
