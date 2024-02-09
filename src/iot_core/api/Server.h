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
  toolbox::strref _contentTypeHeader;
  toolbox::strref _plainArg;
  toolbox::StringInput _stream;

public:
  explicit RequestBody(ESP8266WebServer& server) :
    _contentTypeHeader(server.header(FPSTR(HEADER_CONTENT_TYPE))),
    _plainArg(server.arg("plain")),
    _stream(_plainArg)
  {}

  const toolbox::strref& contentType() const override {
    return _contentTypeHeader;
  }

  const toolbox::strref& content() override {
    return _plainArg;
  }

  size_t available() const override {
    return _stream.available();
  }

  size_t read(char* buffer, size_t bufferSize) override {
    return _stream.read(buffer, bufferSize);
  }

  size_t readString(char* buffer, size_t bufferSize) override {
    return _stream.readString(buffer, bufferSize);
  }
};

class SingleResponseBody final : public IResponseBody {
private:
  ESP8266WebServer& _server;
  int _responseCode;
  toolbox::strref _contentType;
  bool _valid;

public:
  explicit SingleResponseBody(ESP8266WebServer& server) : _server(server), _responseCode(200), _contentType("text/plain"), _valid(false) {}

  void begin(int code, const toolbox::strref& contentType) {
    _responseCode = code;
    _contentType = contentType;
    _valid = true;
  }

  void end() {
    _valid = false;
  }

  bool valid() const override { return _valid; };

  size_t write(const toolbox::strref& content) override {
    if (!_valid) {
      return 0u;
    }

    if (content.isInProgmem() || _contentType.isInProgmem()) {
      _server.send_P(_responseCode, _contentType.ref(), content.ref(), content.length());
    } else {
      _server.send(_responseCode, _contentType.ref(), content.ref(), content.length());
    }
    
    return content.length();
  }

  size_t write(char c) override {
    if (!_valid) {
      return 0u;
    }
    _server.send(_responseCode, _contentType.cstr(), &c, 1);
    return 1u;
  }
};

class ChunkedResponseBody final : public IResponseBody {
private:
  ChunkedResponse<ESP8266WebServer> _response;

public:
  explicit ChunkedResponseBody(ESP8266WebServer& server) : _response(server) {}
  void begin(int code, const toolbox::strref& contentType) { _response.begin(code, contentType); }
  void end() { _response.end(); }
  bool valid() const override { return _response.valid(); };
  size_t write(const toolbox::strref& content) override { return _response.write(content); }
  size_t write(char c) override { return _response.write(c); }
};

class Request final : public IRequest {
private:
  ESP8266WebServer& _server;
  RequestBody _body;

public:
  explicit Request(ESP8266WebServer& server) : _server(server), _body(server) {}

  bool hasArg(const toolbox::strref& name) const override {
    return _server.hasArg(name.toString());
  }

  toolbox::strref arg(const toolbox::strref& name) const override {
    return _server.arg(name.toString());
  }

  toolbox::strref pathArg(unsigned int i) const override {
    return _server.pathArg(i);
  }

  IRequestBody& body() override {
    return _body;
  }
};

class Response final : public IResponse {
private:
  ESP8266WebServer& _server;
  SingleResponseBody _singleBody;
  ChunkedResponseBody _chunkedBody;
  int _code;
  toolbox::strref _contentType;
  
public:
  explicit Response(ESP8266WebServer& server) : _server(server), _singleBody(server), _chunkedBody(server), _code(mapResponseCode(ResponseCode::NotImplemented)), _contentType(mapContentType(ContentType::TextPlain)) {}

  virtual ~Response() {
    if (_singleBody.valid()) {
      _singleBody.end();
    } else if (_chunkedBody.valid()) {
      _chunkedBody.end();
    } else {
      _server.send_P(_code, _contentType.ref(), "");
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

  IResponse& contentType(const toolbox::strref& contentType) override {
    _contentType = contentType;
    return *this;
  }

  IResponse& header(const toolbox::strref& name, const toolbox::strref& value) override {
    _server.sendHeader(name.toString(), value.toString());
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
  Logger _logger;
  ISystem& _system;
  std::vector<IProvider*> _providers;
  ESP8266WebServer _server;
  TimingStatistics<10u> _callStatistics;
  
public:
  Server(ISystem& system, int port = 80) : _logger(system.logger("api")), _system(system), _providers(), _server(port) {}
  
  void on(const Uri& uri, HttpMethod method, std::function<void(IRequest&, IResponse&)> handler) override {
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
    _server.collectHeaders(FPSTR(HEADER_CONTENT_TYPE));

    // generic OPTIONS reply to make "pre-flight" checks work
    on(UriGlob(F("*")), HttpMethod::OPTIONS, [](IRequest&, IResponse& response) {  
      response.code(ResponseCode::OkNoContent).header(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, DELETE, OPTIONS"));
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
